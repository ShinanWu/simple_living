# 通用 API 响应信封（Common Response Envelope）

## 1. 目的

定义所有面向客户端与合作伙伴的 HTTP API（含 JSON 体）的**统一顶层结构**，保证 Web、iOS、Android、小程序与后端在解析与错误处理上行为一致。

## 2. 顶层字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | boolean | 是 | 业务是否成功：`true` 表示可正常消费 `data`（仍可能带非致命 `warnings`） |
| `code` | integer | 是 | 业务结果码；`0` 表示成功，非零含义见 [error-codes.md](./error-codes.md) |
| `message` | string | 是 | 面向开发者或运营的人类可读说明；**不得**作为客户端唯一分支依据 |
| `data` | object \| array \| null | 是 | 成功时为业务负载；失败时推荐为 `null`，除非契约明确约定错误详情结构 |
| `meta` | object | 否 | 与单次请求相关的元信息，见下文 |
| `errors` | array | 否 | 校验或多资源错误时的明细列表，见下文 |
| `warnings` | array | 否 | 非致命提示，不影响 `success === true` 时的主流程 |

**全局不变量：**

- `code === 0` 当且仅当 `success === true`
- `code !== 0` 当且仅当 `success === false`
- `warnings` 仅应出现在 `success === true` 的响应中

### 2.1 `meta` 约定

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `request_id` | string | 推荐 | 服务端生成的单次请求标识，用于排障与串联日志 |
| `trace_id` | string | 否 | 分布式追踪 ID，与网关/APM 对齐时可填 |
| `server_time_ms` | integer | 否 | 服务端 Unix 毫秒时间戳，便于客户端校正展示 |

分页相关字段若存在，须符合 [pagination.md](./pagination.md)，并统一置于 `data.pagination`；**不得**在 `meta` 中随意塞入未文档化的分页键，也不得在 `data` 外另起一套分页结构。

### 2.2 `errors` 单项结构

用于表达字段级或多问题场景，与顶层 `code` 配合使用：

| 字段 | 类型 | 说明 |
|------|------|------|
| `field` | string | 可选；问题字段路径，建议点分或 JSON Pointer 子集，如 `items.2.title` |
| `code` | integer | 可选；更细粒度错误码，仍在 [error-codes.md](./error-codes.md) 约定范围内 |
| `message` | string | 人类可读说明 |

## 3. 示例

### 3.1 成功（无分页）

```json
{
  "success": true,
  "code": 0,
  "message": "OK",
  "data": {
    "guide_card_id": "550e8400-e29b-41d4-a716-446655440000"
  },
  "meta": {
    "request_id": "req_01jqxyz",
    "server_time_ms": 1711603200000
  }
}
```

### 3.2 业务失败

```json
{
  "success": false,
  "code": 20001,
  "message": "Resource not found",
  "data": null,
  "meta": {
    "request_id": "req_01jqabc"
  }
}
```

### 3.3 校验失败（带 `errors`）

```json
{
  "success": false,
  "code": 10002,
  "message": "Validation failed",
  "data": null,
  "errors": [
    { "field": "page_size", "code": 10002, "message": "must be <= 100" }
  ],
  "meta": { "request_id": "req_01jqdef" }
}
```

## 4. 兼容性原则

- 仅允许**新增**可选顶层字段或 `meta` 键；禁止删除或改变已有字段语义。
- 客户端须**忽略**未知字段。

## 5. HTTP 状态码与 `code` 的关系

- 所有对外 HTTP API 都应尽量返回本信封，包括 4xx/5xx。
- `2xx` 仅用于请求被成功处理且 `success === true` 的场景。
- 业务可预期失败（如未登录、无权限、资源不存在、限流）应使用匹配语义的 **4xx + 本信封**，且 `code` 为非零业务码。
- 网关或基础设施错误使用 **5xx + 本信封**；若极端场景无法返回标准体，应在网关层记录审计与监控。
- **禁止**仅依赖 HTTP 状态码表达细粒度业务原因；客户端分支应以 `code` 为准，HTTP 状态码作为大类辅助语义。
