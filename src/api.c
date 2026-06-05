#include "api.h"
#include "resources.h"
#include "tools.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * api.c 实现了 MCP JSON-RPC 请求调度。
 *
 * 主要流程：
 * 1) 注册所有 MCP 方法、工具接口、资源接口
 * 2) 接收 JSON-RPC 请求并解析
 * 3) 验证 jsonrpc、method、params
 * 4) 根据 method 查找对应 handler
 * 5) 执行 handler 并构造 JSON-RPC 响应
 */
#define MAX_METHODS 32

/* MCP 方法注册表。
 * 每一条记录都包含一个方法名与对应的处理函数。
 * 当收到 JSON-RPC 请求时，dispatch_mcp_request 会根据 method 字段查找这里的注册表。
 */
static struct {
    const char *name;
    mcp_handler_t handler;
} method_registry[MAX_METHODS];

/* 查找已经注册的处理函数。如果没有找到返回 NULL。*/
static mcp_handler_t find_handler(const char *method) {
    /* 在线性表中查找已注册的 MCP 方法处理器。*/
    for (int i = 0; i < MAX_METHODS; i++) {
        if (method_registry[i].name == NULL) {
            break;
        }
        if (strcmp(method_registry[i].name, method) == 0) {
            return method_registry[i].handler;
        }
    }
    return NULL;
}

/*
 * 注册一个 MCP 方法处理函数。
 *
 * 扩展模板：
 * 1) 实现一个符合 mcp_handler_t 签名的处理器。
 * 2) 在 mcp_init_default_methods() 或启动时调用 register_mcp_method() 注册。
 * 3) 方法名应唯一，dispatch_mcp_request() 会在请求到达时查找并调用。
 */
void register_mcp_method(const char *method, mcp_handler_t handler) {
    for (int i = 0; i < MAX_METHODS; i++) {
        if (method_registry[i].name == NULL) {
            method_registry[i].name = method;
            method_registry[i].handler = handler;
            return;
        }
    }
}

/* 将 JSON-RPC 请求中的 id 对象序列化为字符串，直接写入 id_buf。*/
static void serialize_jsonrpc_id(const cJSON *id_item, char *id_buf, size_t id_buf_size) {
    if (id_item == NULL) {
        snprintf(id_buf, id_buf_size, "null");
        return;
    }

    /* cJSON_PrintUnformatted 会返回一个已经分配的字符串。*/
    char *id_text = cJSON_PrintUnformatted(id_item);
    if (id_text == NULL) {
        snprintf(id_buf, id_buf_size, "null");
        return;
    }
    snprintf(id_buf, id_buf_size, "%s", id_text);
    cJSON_free(id_text);
}

/* 将 params 项转换成非格式化 JSON 字符串，供处理器使用。
 * 如果 params 缺失则返回默认字符串 "{}"。
 */
static char *serialize_params(const cJSON *params_item) {
    /* 将 params 对象序列化为紧凑 JSON 文本，传递给具体的 MCP 方法处理器。*/
    if (params_item == NULL) {
        char *default_params = malloc(3);
        if (default_params) {
            strcpy(default_params, "{}");
        }
        return default_params;
    }
    return cJSON_PrintUnformatted(params_item);
}

/* 构造一个 JSON-RPC 错误响应。id_item 可以为 NULL，表示响应 id 为 null。*/
static void make_error_response(const cJSON *id_item,
                                int code,
                                const char *message,
                                char *out,
                                size_t out_sz) {
    char id_text[128];
    serialize_jsonrpc_id(id_item, id_text, sizeof(id_text));
    snprintf(out, out_sz,
             "{\"jsonrpc\":\"2.0\",\"mcp\":\"1.0\",\"error\":{\"code\":%d,\"message\":\"%s\"},\"id\":%s}",
             code,
             message,
             id_text);
}

int dispatch_mcp_request(const char *request_json, char *response_buf, size_t response_buf_size) {
    if (request_json == NULL || response_buf == NULL) {
        return 0;
    }

    /* 1) 解析请求 JSON 并验证结构。*/
    cJSON *request = cJSON_Parse(request_json);
    if (request == NULL || !cJSON_IsObject(request)) {
        make_error_response(NULL, -32700, "Parse error: invalid JSON", response_buf, response_buf_size);
        cJSON_Delete(request);
        return 0;
    }

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(request, "id");
    int is_notification = (id_item == NULL);

    cJSON *jsonrpc_item = cJSON_GetObjectItemCaseSensitive(request, "jsonrpc");
    if (!cJSON_IsString(jsonrpc_item) || strcmp(jsonrpc_item->valuestring, "2.0") != 0) {
        if (is_notification) {
            cJSON_Delete(request);
            response_buf[0] = '\0';
            return 1;
        }
        make_error_response(NULL, -32600, "Invalid Request: unsupported JSON-RPC version", response_buf, response_buf_size);
        cJSON_Delete(request);
        return 0;
    }

    cJSON *method_item = cJSON_GetObjectItemCaseSensitive(request, "method");
    if (!cJSON_IsString(method_item)) {
        if (is_notification) {
            cJSON_Delete(request);
            response_buf[0] = '\0';
            return 1;
        }
        make_error_response(NULL, -32600, "Invalid Request: missing method", response_buf, response_buf_size);
        cJSON_Delete(request);
        return 0;
    }

    /* 4) 如果请求携带 params，则将其序列化为字符串，统一传递给 handler。*/
    cJSON *params_item = cJSON_GetObjectItemCaseSensitive(request, "params");
    char *params_text = serialize_params(params_item);
    if (params_text == NULL) {
        if (is_notification) {
            cJSON_Delete(request);
            response_buf[0] = '\0';
            return 1;
        }
        make_error_response(id_item, -32000, "Internal error: cannot serialize params", response_buf, response_buf_size);
        cJSON_Delete(request);
        return 0;
    }

    /* 5) 查找请求指定的方法处理器，并调用它。*/
    mcp_handler_t handler = find_handler(method_item->valuestring);
    if (handler == NULL) {
        /* 如果是通知请求，不需要返回错误响应，直接忽略。*/
        if (is_notification) {
            free(params_text);
            cJSON_Delete(request);
            response_buf[0] = '\0';
            return 1;
        }
        make_error_response(id_item, -32601, "Method not found", response_buf, response_buf_size);
        free(params_text);
        cJSON_Delete(request);
        return 0;
    }

    char result_json[4096] = {0};
    if (!handler(params_text, result_json, sizeof(result_json))) {
        /* 6) 处理器返回 false 表示内部执行失败。*/
        if (is_notification) {
            free(params_text);
            cJSON_Delete(request);
            response_buf[0] = '\0';
            return 1;
        }
        make_error_response(id_item, -32000, "Internal error in handler", response_buf, response_buf_size);
        free(params_text);
        cJSON_Delete(request);
        return 0;
    }

    free(params_text);

    char id_text[128];
    serialize_jsonrpc_id(id_item, id_text, sizeof(id_text));

    /* 7) 构造标准 JSON-RPC 响应，并将 handler 返回的 result 直接嵌入。*/
    snprintf(response_buf,
             response_buf_size,
             "{\"jsonrpc\":\"2.0\",\"mcp\":\"1.0\",\"result\":%s,\"id\":%s}",
             result_json,
             id_text);

    cJSON_Delete(request);
    return 1;
}

static int echo_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    /* echo 处理器直接返回收到的 params JSON，保持原始结构不变。*/
    if (params_json == NULL || params_json[0] == '\0') {
        snprintf(result_buf, result_buf_size, "{}");
        return 1;
    }
    snprintf(result_buf, result_buf_size, "%s", params_json);
    return 1;
}

static int status_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;
    snprintf(result_buf,
             result_buf_size,
             "{\"server\":\"c-mcp-server\",\"protocol\":\"MCP\",\"version\":\"1.0\",\"status\":\"ready\"}");
    return 1;
}

int mcp_init_default_methods(void) {
    /* 注册基础测试方法。*/
    register_mcp_method("echo", echo_handler);
    register_mcp_method("status", status_handler);

    /* 注册 MCP 核心服务方法、工具和资源接口。*/
    register_default_mcp_methods();
    register_default_mcp_tools();
    register_default_mcp_resources();
    register_default_resource_methods();
    return 0;
}
