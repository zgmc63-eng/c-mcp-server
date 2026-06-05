#ifndef MCP_API_H
#define MCP_API_H

#include <stddef.h>

/*
 * MCP API 插件系统。
 *
 * 通过 register_mcp_method() 注册一个方法名和对应的处理函数。
 * dispatch_mcp_request() 会根据请求 JSON 中的 method 调度到注册的处理器。
 * 处理器函数接收 params 的 JSON 字符串，返回 result JSON 字符串。
 */

typedef int (*mcp_handler_t)(const char *params_json, char *result_buf, size_t result_buf_size);

/* 注册一个 MCP JSON-RPC 方法。method 必须是唯一的字符串。
 * 如果注册成功则在内部方法表中保存该处理器。
 */
void register_mcp_method(const char *method, mcp_handler_t handler);

/*
 * 处理一个完整的 JSON-RPC 2.0 请求。
 * request_json 需要是一个完整的 JSON 文本。
 * 成功时将返回 1，并将完整响应写入 response_buf。
 * 失败时返回 0，并写入 JSON-RPC 错误响应。
 */
int dispatch_mcp_request(const char *request_json, char *response_buf, size_t response_buf_size);

/* 初始化默认的 MCP API 方法，例如 echo 和 status。*/
int mcp_init_default_methods(void);

#endif // MCP_API_H
