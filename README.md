# c-mcp-server

这是一个使用 C 语言实现的 MCP 服务器示例。

项目结构:
- `src/`：所有 C 源文件和头文件
- `build/`：中间编译产物
- `c-mcp-server`：最终可执行文件

特性:
- HTTP POST 统一路径 `/mcp/api`
- 支持 Streamable HTTP chunked 响应
- JSON-RPC 2.0 协议
- MCP Version: 2025-11-25
- 使用 cJSON 库解析和构造 JSON
- 模块化 API 注册接口，便于后续扩展
- 已支持 `initialize`、`notifications/initialized`、`tools/list`、`tools/call`

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
- `session/close`：关闭会话

## 扩展 API 方法

在 `src/tools.c` 中添加新的工具定义，并通过 `register_tool()` 注册。

使用 `src/tools.h` 中的 `mcp_tool_t` 定义，可轻松添加新的工具：

- `name`：工具名称
- `description`：工具说明
- `input_schema`：输入 JSON Schema
- `handler`：处理函数
