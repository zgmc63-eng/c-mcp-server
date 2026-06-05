#include "api.h"
#include "tools.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOOLS 16

static const mcp_tool_t *tool_registry[MAX_TOOLS];
static size_t tool_count = 0;

/* 将 cJSON 对象转换为紧凑的 JSON 字符串。
 * 调用者负责释放返回值。
 */
static char *print_json(const cJSON *json) {
    return cJSON_PrintUnformatted(json);
}

int register_tool(const mcp_tool_t *tool) {
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
    return tool_count;
}

const mcp_tool_t *get_registered_tool(size_t index) {
    if (index >= tool_count) {
        return NULL;
    }
    return tool_registry[index];
}

static cJSON *weather_query_schema(void) {
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

void register_default_mcp_methods(void) {
    register_mcp_method("initialize", initialize_handler);
    register_mcp_method("notifications/initialized", notifications_initialized_handler);
    register_mcp_method("tools/list", tools_list_handler);
    register_mcp_method("tools/call", tools_call_handler);
    register_mcp_method("session/close", session_close_handler);
}
