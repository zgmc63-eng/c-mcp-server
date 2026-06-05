# c-mcp-server

这是一个使用 C 语言实现的 MCP 服务器示例。

项目结构:
- `src/`：所有 C 源文件和头文件
- `build/`：中间编译产物
- `c-mcp-server`：最终可执行文件

环境要求:
- Linux / POSIX: 当前实现基于 POSIX 套接字和标准 Unix 头文件（`arpa/inet.h`、`sys/socket.h`、`unistd.h` 等）。
- Windows: 当前代码未直接支持 Windows，需移植网络层到 Winsock 并替换 POSIX 相关系统调用。

Linux 构建依赖:
- `gcc` 或兼容的 C11 编译器
- `make`
- POSIX 开发环境

Windows 构建说明（需移植）:
- 需要使用 MinGW/MSYS2 或 Visual Studio，并替换 POSIX 网络接口为 Winsock。
- 当前项目的 `Makefile` 和 `src/*.c` 仍假设 POSIX 环境。

特性:
- HTTP POST 统一路径 `/mcp/api`
- 支持 Streamable HTTP chunked 响应
- JSON-RPC 2.0 协议
- MCP Version: 2025-11-25
- 使用 cJSON 库解析和构造 JSON
- 模块化 API 注册接口，便于后续扩展
- 已支持 `initialize`、`notifications/initialized`、`tools/list`、`tools/call`、`resources/list`、`resources/read`

## 构建

```bash
make
```

## 运行

```bash
./c-mcp-server
```

服务器默认监听 `http://0.0.0.0:8080/mcp/api`。

## 示例请求

### 初始化会话

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2025-11-25",
    "capabilities": {
      "tools": {
        "listChanged": false
      },
      "resources": {
        "listChanged": false,
        "subscribe": false
      }
    },
    "serverInfo": {
      "name": "c-mcp-server",
      "title": "C MCP Server",
      "version": "1.0.0"
    }
  }
}
```

### 查询可用工具

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "tools": [
      {
        "name": "weather_query",
        "title": "Query weather information",
        "description": "Query weather information",
        "execution": {
          "taskSupport": "forbidden"
        },
        "inputSchema": {
          "type": "object",
          "properties": {
            "city": {"type": "string"},
            "unit": {"type": "string"}
          },
          "required": ["city"]
        }
      }
    ]
  }
}
```

### 查询可用资源

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"resources/list","params":{}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "resources": [
      {
        "name": "server_info",
        "title": "MCP server capabilities",
        "description": "A text resource describing the server's available MCP interfaces.",
        "uri": "resource://c-mcp-server/server_info",
        "mimeType": "text/plain"
      }
    ]
  }
}
```

### 读取资源

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":4,"method":"resources/read","params":{"uri":"resource://c-mcp-server/server_info"}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 4,
  "result": {
    "contents": [
      {
        "uri": "resource://c-mcp-server/server_info",
        "text": "This server supports initialize, tools/list, tools/call, resources/list, resources/read, and session/close. Use resources/list to discover available resources and resources/read to fetch text content.",
        "mimeType": "text/plain"
      }
    ]
  }
}
```

### 调用工具

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"weather_query","arguments":{"city":"Tokyo","unit":"celsius"}}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Tokyo:26°C,Sunny,Humidity65%"
      }
    ],
    "isError": false
  }
}
```

### 结束会话

```bash
curl -X POST http://127.0.0.1:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"session/close","params":{}}'
```

响应示例:

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "status": "closed"
  }
}
```

## 完整服务接口

当前实现包含以下 MCP 服务接口：

- `initialize`：初始化 MCP 会话
- `notifications/initialized`：客户端初始化完成通知
- `tools/list`：发现可用工具
- `tools/call`：调用注册工具
- `resources/list`：发现可用资源
- `resources/read`：读取指定资源内容
- `session/close`：关闭会话

## 扩展 API 方法

在 `src/tools.c` 中添加新的工具定义，并通过 `register_tool()` 注册。

使用 `src/tools.h` 中的 `mcp_tool_t` 定义，可轻松添加新的工具：

- `name`：工具名称
- `description`：工具说明
- `input_schema`：输入 JSON Schema
- `handler`：处理函数

### 扩展接口模板

1. 实现一个新的 MCP 方法处理函数。签名如下：

```c
static int my_new_method_handler(const char *params_json, char *result_buf, size_t result_buf_size) {
    cJSON *params = cJSON_Parse(params_json);
    if (!params || !cJSON_IsObject(params)) {
      cJSON_Delete(params);
      snprintf(result_buf, result_buf_size, "{}");
    return 1;
  }

  /* 解析 params，并构造 result */
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "message", "ok");

  char *result_text = cJSON_PrintUnformatted(result);
  cJSON_Delete(result);
  if (!result_text) {
    cJSON_Delete(params);
    return 0;
  }

  snprintf(result_buf, result_buf_size, "%s", result_text);
  cJSON_free(result_text);
  cJSON_Delete(params);
  return 1;
}
```

2. 在 `mcp_init_default_methods()` 中注册该方法：

```c
register_mcp_method("my/new_method", my_new_method_handler);
```

3. 如果要添加工具：
   - 在 `src/tools.c` 中定义 `mcp_tool_t`。
   - 提供 `input_schema()` 和 `handler()`。
   - 调用 `register_tool(&my_tool)` 注册。
   - `tools/list` 会自动返回已注册工具，`tools/call` 将调用 handler。

4. 如果要添加资源：
   - 在 `src/resources.c` 中定义 `mcp_resource_t`。
   - 调用 `register_resource(&my_resource)` 注册。
   - `resources/list` 会返回资源 metadata，`resources/read` 会读取资源内容。

### 注意事项

 - `dispatch_mcp_request()` 会自动识别通知请求（没有 `id`），并忽略其错误返回。
 - 方法名必须唯一，重复注册会导致查找错误。
 - handler 必须返回完整的 JSON 结果文本，不要在 `result_buf` 中写入额外换行。
 - `register_tool()` 和 `register_resource()` 有固定最大数量限制，如超出则注册失败。
 - tool handler 的 `result_obj` 必须写入符合 `tools/call` 结果结构的 JSON。
 - 任何内部失败应返回 `0`，以便 `dispatch_mcp_request()` 生成标准 JSON-RPC 错误。

## 完整代码样板（可复制使用）

下面给出可直接复制到项目中的最小可用样板，分别用于添加一个新的工具和一个新的资源。

### 工具（Tool）样板

将以下代码片段添加到 `src/tools.c`（或新建文件并在初始化时注册）：

```c
/* 示例：一个简单的 echo_tool */
static cJSON *echo_tool_schema(void) {
  cJSON *schema = cJSON_CreateObject();
  cJSON_AddStringToObject(schema, "type", "object");
  cJSON *props = cJSON_CreateObject();
  cJSON *text = cJSON_CreateObject();
  cJSON_AddStringToObject(text, "type", "string");
  cJSON_AddItemToObject(props, "text", text);
  cJSON_AddItemToObject(schema, "properties", props);
  return schema; /* 调用者负责释放 */
}

static int echo_tool_handler(const cJSON *arguments, cJSON *result_obj) {
  if (!cJSON_IsObject(arguments) || result_obj == NULL) return 0;
  cJSON *text = cJSON_GetObjectItemCaseSensitive(arguments, "text");
  if (!cJSON_IsString(text) || !text->valuestring) return 0;

  /* 将工具执行结果写入 result_obj，tools/call 期望的结构示例：*/
  cJSON *content = cJSON_CreateArray();
  cJSON *item = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "type", "text");
  cJSON_AddStringToObject(item, "text", text->valuestring);
  cJSON_AddItemToArray(content, item);

  cJSON_AddItemToObject(result_obj, "content", content);
  cJSON_AddBoolToObject(result_obj, "isError", 0);
  return 1;
}

static const mcp_tool_t echo_tool = {
  .name = "echo_tool",
  .description = "Return provided text",
  .input_schema = echo_tool_schema,
  .handler = echo_tool_handler
};

/* 在初始化路径（例如 mcp_init_default_methods）中调用： */
/* register_tool(&echo_tool); */
```

注意：`echo_tool_schema()` 返回的 `cJSON *` 会被 `make_tool_list_json` 直接加入响应并在最终释放调用方负责释放。

### 资源（Resource）样板

将以下代码片段添加到 `src/resources.c`（或新建文件并在初始化时注册）：

```c
/* 示例：一个简单的 static_text 资源 */
static const mcp_resource_t my_text_resource = {
  .name = "my_text",
  .title = "示例文本资源",
  .description = "一个用于展示的静态文本资源",
  .uri = "resource://c-mcp-server/my_text",
  .mimeType = "text/plain",
  .text = "这是一个示例资源的文本内容。"
};

/* 在初始化路径调用以注册资源： */
/* register_resource(&my_text_resource); */
```

资源注册后：
- `resources/list` 将包含该资源的 metadata
- `resources/read` 将返回 `contents` 数组，其中包含 `uri`、`text` 和可选的 `mimeType`

### 注册和初始化位置

建议将上述 `register_tool()`、`register_resource()` 调用放在 `mcp_init_default_methods()` 中或在 `main()` 启动阶段调用，以确保所有方法、工具和资源在服务器开始接受请求前都已注册。

