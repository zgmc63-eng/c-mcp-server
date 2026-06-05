#ifndef TOOLS_H
#define TOOLS_H

#include "cJSON.h"
#include <stddef.h>

/*
 * MCP 工具处理函数。
 * arguments: 从 tools/call params.arguments 提取的 JSON 对象。
 * result_obj: 由调用者创建并传入，处理函数应将 output 写入该对象。
 * 返回 1 表示成功，0 表示失败。
 */
typedef int (*tool_handler_t)(const cJSON *arguments, cJSON *result_obj);

/* 返回一个描述输入参数结构的 JSON Schema 对象。
 * 调用者会负责释放该对象。
 */
typedef cJSON *(*tool_schema_fn)(void);

typedef struct {
    const char *name;
    const char *description;
    tool_schema_fn input_schema;
    tool_handler_t handler;
} mcp_tool_t;

void register_default_mcp_methods(void);
void register_default_mcp_tools(void);
int register_tool(const mcp_tool_t *tool);
const mcp_tool_t *find_tool(const char *name);
size_t get_registered_tool_count(void);
const mcp_tool_t *get_registered_tool(size_t index);

#endif // TOOLS_H
