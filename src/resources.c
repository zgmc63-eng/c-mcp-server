#include "api.h"
#include "resources.h"
#include "cJSON.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RESOURCES 16

/* 资源注册表：保存可由 resources/list 和 resources/read 查询的资源定义。*/
static const mcp_resource_t *resource_registry[MAX_RESOURCES];
static size_t resource_count = 0;

/* 将 cJSON 对象序列化为紧凑 JSON 文本，供响应体使用。*/
static char *serialize_json(const cJSON *json) {
    return cJSON_PrintUnformatted(json);
}

int register_resource(const mcp_resource_t *resource) {
    /* 注册资源时要求至少提供 name 与 uri，且不能重复。*/
    if (resource == NULL || resource->name == NULL || resource->uri == NULL) {
        return 0;
    }
    if (resource_count >= MAX_RESOURCES) {
        return 0;
    }
    for (size_t i = 0; i < resource_count; i++) {
        if (strcmp(resource_registry[i]->uri, resource->uri) == 0) {
            return 0;
        }
    }
    resource_registry[resource_count++] = resource;
    return 1;
}

const mcp_resource_t *find_resource(const char *uri) {
    /* 在资源注册表中查找指定 URI 的资源定义。*/
    if (uri == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < resource_count; i++) {
        if (strcmp(resource_registry[i]->uri, uri) == 0) {
            return resource_registry[i];
        }
    }
    return NULL;
}

size_t get_registered_resource_count(void) {
    /* 返回当前注册资源数量。*/
    return resource_count;
}

const mcp_resource_t *get_registered_resource(size_t index) {
    /* 根据索引获取已注册资源，越界时返回 NULL。*/
    if (index >= resource_count) {
        return NULL;
    }
    return resource_registry[index];
}

static int resources_list_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    (void)params_json;

    /* 处理 resources/list 请求，返回所有已注册资源的 metadata 列表。*/
    cJSON *result = cJSON_CreateObject();
    cJSON *resources_array = cJSON_CreateArray();
    cJSON_AddItemToObject(result, "resources", resources_array);

    for (size_t i = 0; i < get_registered_resource_count(); i++) {
        const mcp_resource_t *resource = get_registered_resource(i);
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", resource->name);
        if (resource->title) {
            cJSON_AddStringToObject(entry, "title", resource->title);
        }
        if (resource->description) {
            cJSON_AddStringToObject(entry, "description", resource->description);
        }
        cJSON_AddStringToObject(entry, "uri", resource->uri);
        if (resource->mimeType) {
            cJSON_AddStringToObject(entry, "mimeType", resource->mimeType);
        }
        cJSON_AddItemToArray(resources_array, entry);
    }

    char *json_text = serialize_json(result);
    cJSON_Delete(result);
    if (!json_text) {
        return 0;
    }
    size_t len = strlen(json_text);
    if (len >= result_buf_size) {
        cJSON_free(json_text);
        return 0;
    }
    memcpy(result_buf, json_text, len + 1);
    cJSON_free(json_text);
    return 1;
}

static int resources_read_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    /* 处理 resources/read 请求，读取指定 URI 的资源内容。*/
    cJSON *params = NULL;
    cJSON *result = NULL;
    char *json_text = NULL;
    int success = 0;

    params = cJSON_Parse(params_json);
    if (params == NULL || !cJSON_IsObject(params)) {
        const char *empty = "{\"contents\":[]}";
        size_t el = strlen(empty);
        if (el < result_buf_size) {
            memcpy(result_buf, empty, el + 1);
            success = 1;
        }
        goto cleanup;
    }

    cJSON *uri_item = cJSON_GetObjectItemCaseSensitive(params, "uri");
    if (!cJSON_IsString(uri_item) || uri_item->valuestring == NULL) {
        const char *empty = "{\"contents\":[]}";
        size_t el2 = strlen(empty);
        if (el2 < result_buf_size) {
            memcpy(result_buf, empty, el2 + 1);
            success = 1;
        }
        goto cleanup;
    }

    const mcp_resource_t *resource = find_resource(uri_item->valuestring);
    result = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();
    cJSON_AddItemToObject(result, "contents", contents);

    if (resource && resource->text) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "uri", resource->uri);
        cJSON_AddStringToObject(item, "text", resource->text);
        if (resource->mimeType) {
            cJSON_AddStringToObject(item, "mimeType", resource->mimeType);
        }
        cJSON_AddItemToArray(contents, item);
    }

    json_text = serialize_json(result);
    if (!json_text) {
        goto cleanup;
    }
    size_t len3 = strlen(json_text);
    if (len3 >= result_buf_size) {
        goto cleanup;
    }
    memcpy(result_buf, json_text, len3 + 1);
    success = 1;

cleanup:
    if (json_text) {
        cJSON_free(json_text);
    }
    if (result) {
        cJSON_Delete(result);
    }
    if (params) {
        cJSON_Delete(params);
    }
    return success;
}

/* 默认注册的资源示例：server_info。*/
static const mcp_resource_t server_info_resource = {
    .name = "server_info",
    .title = "MCP server capabilities",
    .description = "A text resource describing the server's available MCP interfaces.",
    .uri = "resource://c-mcp-server/server_info",
    .mimeType = "text/plain",
    .text = "This server supports initialize, tools/list, tools/call, resources/list, resources/read, and session/close. Use resources/list to discover available resources and resources/read to fetch text content."
};

void register_default_mcp_resources(void) {
    register_resource(&server_info_resource);
}

void register_default_resource_methods(void) {
    register_mcp_method("resources/list", resources_list_handler);
    register_mcp_method("resources/read", resources_read_handler);
}

/*
 * 可复制的资源示例模板：
 * 将 README 中的资源样板复制到此文件或其它源文件，并在初始化阶段调用 register_resource().
 */
