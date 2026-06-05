#include "api.h"
#include "tools.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOOLS 16

/* 工具注册表：一个简单的静态数组，用于保存可调用的 MCP 工具定义。*/
static const mcp_tool_t *tool_registry[MAX_TOOLS];
static size_t tool_count = 0;

/* 将 cJSON 对象转换为紧凑的 JSON 字符串。
 * 这个字符串会被写入 MCP response 结果里。
 * 调用者负责释放返回值。
 */
static char *print_json(const cJSON *json) {
    return cJSON_PrintUnformatted(json);
}

int register_tool(const mcp_tool_t *tool) {
    /* 注册一个新的 MCP 工具，防止重复注册。*/
    if (tool == NULL || tool->name == NULL || tool->handler == NULL) {
        return 0;
    }
    if (tool_count >= MAX_TOOLS) {
        return 0;
    }
    for (size_t i = 0; i < tool_count; i++) {
        if (strcmp(tool_registry[i]->name, tool->name) == 0) {
            return 0;
        }
    }
    tool_registry[tool_count++] = tool;
    return 1;
}

const mcp_tool_t *find_tool(const char *name) {
    /* 根据工具名称查找已注册工具，返回匹配项或 NULL。*/
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < tool_count; i++) {
        if (strcmp(tool_registry[i]->name, name) == 0) {
            return tool_registry[i];
        }
    }
    return NULL;
}

size_t get_registered_tool_count(void) {
    /* 返回当前注册的工具数量。*/
    return tool_count;
}

const mcp_tool_t *get_registered_tool(size_t index) {
    /* 根据索引返回已注册工具，越界时返回 NULL。*/
    if (index >= tool_count) {
        return NULL;
    }
    return tool_registry[index];
}

static cJSON *weather_query_schema(void) {
    /* 返回 weather_query 工具的参数 JSON Schema。
     * 该 schema 用于 tools/list 响应中的 tool.inputSchema 字段。
     */
    cJSON *schema = cJSON_CreateObject();
    cJSON_AddStringToObject(schema, "type", "object");

    cJSON *properties = cJSON_CreateObject();
    cJSON *city_field = cJSON_CreateObject();
    cJSON_AddStringToObject(city_field, "type", "string");
    cJSON_AddItemToObject(properties, "city", city_field);

    cJSON *unit_field = cJSON_CreateObject();
    cJSON_AddStringToObject(unit_field, "type", "string");
    cJSON_AddItemToObject(properties, "unit", unit_field);

    cJSON_AddItemToObject(schema, "properties", properties);
    cJSON *required = cJSON_CreateArray();
    cJSON_AddItemToArray(required, cJSON_CreateString("city"));
    cJSON_AddItemToObject(schema, "required", required);
    return schema;
}

static int weather_query_handler(const cJSON *arguments, cJSON *result_obj) {
    /* 处理 weather_query 工具调用，返回简化天气文本结果。*/
    if (!cJSON_IsObject(arguments) || result_obj == NULL) {
        return 0;
    }

    cJSON *city_item = cJSON_GetObjectItemCaseSensitive(arguments, "city");
    if (!cJSON_IsString(city_item) || city_item->valuestring == NULL) {
        return 0;
    }

    cJSON *unit_item = cJSON_GetObjectItemCaseSensitive(arguments, "unit");
    const char *unit = "celsius";
    if (cJSON_IsString(unit_item) && unit_item->valuestring != NULL) {
        unit = unit_item->valuestring;
    }
    const char *unit_symbol = strcmp(unit, "fahrenheit") == 0 ? "°F" : "°C";

    char weather_text[256];
    snprintf(weather_text, sizeof(weather_text), "%s:26%s,Sunny,Humidity65%%", city_item->valuestring, unit_symbol);

    cJSON *content = cJSON_CreateArray();
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "type", "text");
    cJSON_AddStringToObject(entry, "text", weather_text);
    cJSON_AddItemToArray(content, entry);

    cJSON_AddItemToObject(result_obj, "content", content);
    cJSON_AddBoolToObject(result_obj, "isError", 0);
    return 1;
}

static void make_tool_list_json(cJSON *tools_array) {
    /* 将每个已注册工具转换成 tools/list 响应中的 JSON 对象。*/
    size_t count = get_registered_tool_count();
    for (size_t i = 0; i < count; i++) {
        const mcp_tool_t *tool = get_registered_tool(i);
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", tool->name);
        cJSON_AddStringToObject(entry, "title", tool->description);
        cJSON_AddStringToObject(entry, "description", tool->description);

        cJSON *execution = cJSON_CreateObject();
        cJSON_AddStringToObject(execution, "taskSupport", "forbidden");
        cJSON_AddItemToObject(entry, "execution", execution);

        if (tool->input_schema) {
            cJSON *schema = tool->input_schema();
            if (schema) {
                cJSON_AddItemToObject(entry, "inputSchema", schema);
            }
        }
        cJSON_AddItemToArray(tools_array, entry);
    }
}

static int initialize_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "protocolVersion", "2025-11-25");
    cJSON_AddStringToObject(result, "instructions", "Use tools/list to discover server tools and tools/call to invoke them.");

    cJSON *capabilities = cJSON_CreateObject();
    cJSON *tools_capabilities = cJSON_CreateObject();
    cJSON_AddBoolToObject(tools_capabilities, "listChanged", 0);
    cJSON_AddItemToObject(capabilities, "tools", tools_capabilities);

    cJSON *resources_capabilities = cJSON_CreateObject();
    cJSON_AddBoolToObject(resources_capabilities, "listChanged", 0);
    cJSON_AddBoolToObject(resources_capabilities, "subscribe", 0);
    cJSON_AddItemToObject(capabilities, "resources", resources_capabilities);

    cJSON_AddItemToObject(result, "capabilities", capabilities);

    cJSON *server_info = cJSON_CreateObject();
    cJSON_AddStringToObject(server_info, "name", "c-mcp-server");
    cJSON_AddStringToObject(server_info, "title", "C MCP Server");
    cJSON_AddStringToObject(server_info, "version", "1.0.0");
    cJSON_AddStringToObject(server_info, "description", "A minimal C-based Model Context Protocol server with tools/list and tools/call support.");
    cJSON_AddItemToObject(result, "serverInfo", server_info);

    char *json_text = print_json(result);
    cJSON_Delete(result);
    if (!json_text) {
        return 0;
    }
    snprintf(result_buf, result_buf_size, "%s", json_text);
    cJSON_free(json_text);
    return 1;
}

static int notifications_initialized_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;
    snprintf(result_buf, result_buf_size, "{}");
    return 1;
}

static int tools_list_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;

    /* 处理 tools/list 请求，返回当前注册的工具列表。*/
    cJSON *result = cJSON_CreateObject();
    cJSON *tools_array = cJSON_CreateArray();
    cJSON_AddItemToObject(result, "tools", tools_array);
    make_tool_list_json(tools_array);

    char *json_text = print_json(result);
    cJSON_Delete(result);
    if (!json_text) {
        return 0;
    }
    snprintf(result_buf, result_buf_size, "%s", json_text);
    cJSON_free(json_text);
    return 1;
}

static int tools_call_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    /* 解析 tools/call 请求参数并查找目标工具。*/
    cJSON *params = cJSON_Parse(params_json);
    if (params == NULL || !cJSON_IsObject(params)) {
        snprintf(result_buf, result_buf_size, "{\"content\":[],\"isError\":true,\"error\":\"Invalid params\"}");
        cJSON_Delete(params);
        return 1;
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(params, "name");
    cJSON *arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments");
    cJSON *arguments_owned = NULL;
    if (arguments == NULL) {
        /* 如果未传递 arguments，则创建一个空对象，避免 NULL 处理。*/
        arguments_owned = cJSON_CreateObject();
        arguments = arguments_owned;
    }
    if (!cJSON_IsString(name) || name->valuestring == NULL || !cJSON_IsObject(arguments)) {
        snprintf(result_buf, result_buf_size, "{\"content\":[],\"isError\":true,\"error\":\"Missing tool name or arguments\"}");
        cJSON_Delete(arguments_owned);
        cJSON_Delete(params);
        return 1;
    }

    const mcp_tool_t *tool = find_tool(name->valuestring);
    if (tool == NULL) {
        snprintf(result_buf, result_buf_size, "{\"content\":[],\"isError\":true,\"error\":\"Unknown tool\"}");
        cJSON_Delete(arguments_owned);
        cJSON_Delete(params);
        return 1;
    }

    cJSON *tool_result = cJSON_CreateObject();
    if (!tool_result) {
        cJSON_Delete(arguments_owned);
        cJSON_Delete(params);
        return 0;
    }

    /* 调用工具处理函数并将结果封装进 tool_result。*/
    if (!tool->handler(arguments, tool_result)) {
        cJSON_Delete(tool_result);
        snprintf(result_buf, result_buf_size, "{\"content\":[],\"isError\":true,\"error\":\"Tool execution failed\"}");
        cJSON_Delete(arguments_owned);
        cJSON_Delete(params);
        return 1;
    }

    char *json_text = print_json(tool_result);
    cJSON_Delete(tool_result);
    cJSON_Delete(params);
    cJSON_Delete(arguments_owned);
    if (!json_text) {
        return 0;
    }
    snprintf(result_buf, result_buf_size, "%s", json_text);
    cJSON_free(json_text);
    return 1;
}

static int session_close_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "closed");
    char *json_text = print_json(result);
    cJSON_Delete(result);
    if (!json_text) {
        return 0;
    }
    snprintf(result_buf, result_buf_size, "%s", json_text);
    cJSON_free(json_text);
    return 1;
}

void register_default_mcp_tools(void) {
    static const mcp_tool_t weather_tool = {
        .name = "weather_query",
        .description = "Query weather information",
        .input_schema = weather_query_schema,
        .handler = weather_query_handler,
    };
    register_tool(&weather_tool);
}

/*
 * 可复制的工具示例模板：
 * 将以下注释示例复制到本文件或新的源文件中并在初始化时调用 register_tool(&your_tool);
 * 代码示例参见 README 中的工具样板。
 */

void register_default_mcp_methods(void) {
    register_mcp_method("initialize", initialize_handler);
    register_mcp_method("notifications/initialized", notifications_initialized_handler);
    register_mcp_method("tools/list", tools_list_handler);
    register_mcp_method("tools/call", tools_call_handler);
    register_mcp_method("session/close", session_close_handler);
}
