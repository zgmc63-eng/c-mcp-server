#include "api.h"
#include "http_server.h"
#include <stdio.h>

/*
 * 程序入口。
 * 初始化默认 MCP 方法后启动 HTTP 服务器。
 */
int main(void) {
    if (mcp_init_default_methods() != 0) {
        fprintf(stderr, "Failed to initialize MCP methods\n");
        return 1;
    }

    /* 默认监听 8080 端口，统一路径为 /mcp/api。*/
    return run_mcp_server(8080);
}
