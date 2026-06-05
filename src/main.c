#include "api.h"
#include "http_server.h"
#include <stdio.h>

/*
 * 程序入口。
 * 先初始化 MCP 默认方法、工具和资源接口，然后启动一个监听 HTTP POST /mcp/api 的服务器。
 */
int main(void) {
    if (mcp_init_default_methods() != 0) {
        fprintf(stderr, "Failed to initialize MCP methods\n");
        return 1;
    }

    /* 默认监听 8080 端口，统一路径为 /mcp/api。*/
    return run_mcp_server(8080);
}
