#include "http_server.h"
#include "api.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * 发送指定长度的数据到 socket，确保全部写入。
 * 这是一个简单的 write 全局封装函数。
 */
static int send_all(int sock, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/*
 * 以 HTTP Chunked Transfer Encoding 方式分块发送字符串。
 * 由于最终 JSON 响应长度动态生成，这里不需要预先计算 Content-Length，
 * 而是逐块写入数据，并在末尾写入 "0\r\n\r\n" 结束标记。
 */
static int send_chunked_data(int sock, const char *data) {
    const size_t chunk_size = 128;
    size_t len = strlen(data);
    const char *p = data;

    while (len > 0) {
        size_t current = len < chunk_size ? len : chunk_size;
        char header[32];
        int header_len = snprintf(header, sizeof(header), "%zx\r\n", current);
        if (send_all(sock, header, (size_t)header_len) < 0) {
            return -1;
        }
        if (send_all(sock, p, current) < 0) {
            return -1;
        }
        if (send_all(sock, "\r\n", 2) < 0) {
            return -1;
        }
        p += current;
        len -= current;
    }
    return send_all(sock, "0\r\n\r\n", 5);
}

/*
 * 跳过 HTTP 头部中的空白字符。
 */
static const char *skip_space(const char *p) {
    while (*p && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t')) {
        p++;
    }
    return p;
}

/*
 * 从 HTTP 请求头第一行解析出 METHOD 和 PATH。
 * 例如 "POST /mcp/api HTTP/1.1"。
 */
static int parse_request_line(const char *header, char *method, size_t method_sz, char *path, size_t path_sz) {
    const char *line_end = strstr(header, "\r\n");
    if (!line_end) {
        return 0;
    }

    const char *p = header;
    const char *space = strchr(p, ' ');
    if (!space || space >= line_end) {
        return 0;
    }

    size_t mlen = (size_t)(space - p);
    if (mlen >= method_sz) {
        return 0;
    }
    memcpy(method, p, mlen);
    method[mlen] = '\0';

    p = skip_space(space + 1);
    space = strchr(p, ' ');
    if (!space || space >= line_end) {
        return 0;
    }

    size_t plen = (size_t)(space - p);
    if (plen >= path_sz) {
        return 0;
    }
    memcpy(path, p, plen);
    path[plen] = '\0';
    return 1;
}

/*
 * 从 HTTP 头部中查找指定字段名的值，例如 Content-Length。
 * 返回 1 表示找到对应值，0 表示未找到或缓冲区不足。
 */
static int get_header_value(const char *header, const char *name, char *value, size_t value_sz) {
    const char *p = header;
    size_t name_len = strlen(name);

    while (*p) {
        if (strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
            p += name_len + 1;
            p = skip_space(p);
            const char *end = strstr(p, "\r\n");
            if (!end) {
                return 0;
            }
            size_t len = (size_t)(end - p);
            if (len >= value_sz) {
                return 0;
            }
            memcpy(value, p, len);
            value[len] = '\0';
            return 1;
        }
        const char *next = strstr(p, "\r\n");
        if (!next) {
            break;
        }
        p = next + 2;
    }
    return 0;
}

/*
 * 读取 HTTP 头部直到遇到 CRLF CRLF 结束标记。
 * HTTP 请求头部可能分多次到达，此函数循环读取直到完整头部被接收。
 */
static int receive_header(int sock, char *buffer, size_t buffer_sz) {
    size_t offset = 0;
    while (offset + 1 < buffer_sz) {
        ssize_t n = recv(sock, buffer + offset, 1, 0);
        if (n <= 0) {
            return -1;
        }
        offset += (size_t)n;
        if (offset >= 4 &&
            buffer[offset - 4] == '\r' &&
            buffer[offset - 3] == '\n' &&
            buffer[offset - 2] == '\r' &&
            buffer[offset - 1] == '\n') {
            buffer[offset] = '\0';
            return (int)offset;
        }
    }
    return -1;
}

/*
 * 读取指定长度的请求体内容，并确保完整接收。
 * 该函数用于 Content-Length 已知的 POST JSON 身体。
 */
static int receive_exact(int sock, char *buffer, size_t length) {
    size_t received = 0;
    while (received < length) {
        ssize_t n = recv(sock, buffer + received, length - received, 0);
        if (n <= 0) {
            return -1;
        }
        received += (size_t)n;
    }
    return 0;
}

/*
 * 发送一个简单的 HTTP JSON 响应，适用于错误情况。
 */
static void send_simple_response(int sock, int status_code, const char *status_text, const char *json_body) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: application/json; charset=utf-8\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status_code,
                              status_text,
                              strlen(json_body));
    send_all(sock, header, (size_t)header_len);
    send_all(sock, json_body, strlen(json_body));
}

/*
 * 处理单个客户端请求：解析 HTTP 头部、读取请求体、调度 MCP JSON-RPC。
 */
/*
 * 处理客户端连接的完整流程：
 * 1) 读取并解析 HTTP 请求头
 * 2) 只支持 POST /mcp/api
 * 3) 读取 Content-Length 指定的请求体
 * 4) 将请求体传给 MCP dispatch
 * 5) 以 chunked transfer 方式发送 JSON-RPC 响应
 */
static void handle_client(int client_sock) {
    char header[8192];
    int header_len = receive_header(client_sock, header, sizeof(header));
    if (header_len <= 0) {
        close(client_sock);
        return;
    }

    char method[16] = {0};
    char path[256] = {0};
    if (!parse_request_line(header, method, sizeof(method), path, sizeof(path))) {
        send_simple_response(client_sock, 400, "Bad Request", "{\"error\":\"Invalid request line\"}");
        close(client_sock);
        return;
    }

    /* 仅支持 POST /mcp/api，其他请求返回 404。*/
    if (strcmp(method, "POST") != 0 || strcmp(path, "/mcp/api") != 0) {
        send_simple_response(client_sock, 404, "Not Found", "{\"error\":\"Endpoint not found\"}");
        close(client_sock);
        return;
    }

    char content_length_value[32] = {0};
    if (!get_header_value(header, "Content-Length", content_length_value, sizeof(content_length_value))) {
        send_simple_response(client_sock, 411, "Length Required", "{\"error\":\"Content-Length required\"}");
        close(client_sock);
        return;
    }

    size_t content_length = (size_t)atoi(content_length_value);
    if (content_length == 0 || content_length > 65536) {
        send_simple_response(client_sock, 400, "Bad Request", "{\"error\":\"Invalid Content-Length\"}");
        close(client_sock);
        return;
    }

    char *body = malloc(content_length + 1);
    if (!body) {
        /* 内存分配失败时直接返回 500 错误码。*/
        send_simple_response(client_sock, 500, "Internal Server Error", "{\"error\":\"Allocation failed\"}");
        close(client_sock);
        return;
    }

    if (receive_exact(client_sock, body, content_length) < 0) {
        free(body);
        close(client_sock);
        return;
    }
    body[content_length] = '\0';

    char response[8192];
    if (!dispatch_mcp_request(body, response, sizeof(response))) {
        /* dispatch_mcp_request 已在 response 中写入错误响应。*/
    }
    free(body);

    char header_buf[512];
    int header_bytes = snprintf(header_buf, sizeof(header_buf),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json; charset=utf-8\r\n"
                                "Transfer-Encoding: chunked\r\n"
                                "Connection: close\r\n"
                                "\r\n");
    if (send_all(client_sock, header_buf, (size_t)header_bytes) < 0) {
        close(client_sock);
        return;
    }
    send_chunked_data(client_sock, response);
    close(client_sock);
}

/*
 * 监听 TCP 端口并不断接受客户端连接。
 */
int run_mcp_server(unsigned short port) {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 16) < 0) {
        perror("listen");
        close(server_sock);
        return 1;
    }

    printf("MCP server listening on http://0.0.0.0:%u/mcp/api\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_sock);
    }

    close(server_sock);
    return 0;
}
