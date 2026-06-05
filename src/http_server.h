#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/*
 * 启动一个简单的 HTTP 服务，接收 MCP JSON-RPC 请求。
 * 该服务会在指定端口上监听，并处理 POST /mcp/api 请求。
 */
int run_mcp_server(unsigned short port);

#endif // HTTP_SERVER_H
