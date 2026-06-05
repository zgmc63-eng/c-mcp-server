# c-mcp-server 远程测试示例

本文件提供通过远程主机访问 `c-mcp-server` 的测试示例。假设服务器运行在远程地址 `example.com:8080`，并且 `/mcp/api` 可访问。

> 当前 `c-mcp-server` 实现是普通 HTTP 服务，不支持 HTTPS/TLS。请务必使用 `http://` 访问。

## 1. 初始化会话

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
```

## 2. 查询可用工具

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'
```

## 3. 调用工具

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"weather_query","arguments":{"city":"Tokyo","unit":"celsius"}}}'
```

## 4. 查询可用资源

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":4,"method":"resources/list","params":{}}'
```

## 5. 读取资源

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":5,"method":"resources/read","params":{"uri":"resource://c-mcp-server/server_info"}}'
```

## 6. 结束会话

```bash
curl -X POST http://example.com:8080/mcp/api \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":6,"method":"session/close","params":{}}'
```

## 注意事项

- 确认远程服务器网络可达，必要时允许防火墙端口 `8080`。
- 如果服务器使用自签名证书，可在 `curl` 中添加 `-k` 选项。
- 所有请求均为 JSON-RPC 2.0 格式，并返回 `jsonrpc`、`id`、`result` 等字段。
