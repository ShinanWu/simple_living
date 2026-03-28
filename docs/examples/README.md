# 示例约定

本文件定义仓库内文档示例的统一写法，目标是让不同服务、不同 Agent 补写文档时保持同一套样例语义，而不是各自发明占位数据。

## 1. 适用范围

- `services/*/docs/api.md` 中的内部 RPC 示例
- `services/gateway/docs/api.md` 中的对外 HTTP / JSON 示例
- 未来新增的协议、页面聚合、事件载荷示例

## 2. 协议示例格式

- **内部 `proto` / RPC**：使用 `textproto`
- **对外 HTTP**：请求使用 `http` 代码块，响应使用 `json` 代码块
- **不要**在内部 RPC 文档中继续使用 “JSON-like proto” 示例
- **不要**在 `gateway` 对外文档中用 `textproto` 替代真实 JSON

## 3. 跨文档共享样例实体

以下标识符在不同服务文档中默认表示同一对象：

- `user_id`: `user_01HSYQK5PZ4K4J9R2M8D`
- `session_id`: `sess_01HSYQBY1S7W4T1M7Q2B`
- `guide_card_id`: `guide_card_1001`
- `favorite_id`: `fav_01HSYQPDYX4B7C8WQ6ZK`
- `recommendation_id`: `rec_01HSZ15H0N8HG9P5P2E0`
- `signal_bundle_ref`: `sigref_01HSZ12V0N6A8Q0X1N2M`
- `click_id`: `click_01HSZ3Y8W7W5S5Y6R3B1`

如无明确必要，新增示例优先复用这些 ID，而不是重新发明新的短 ID。

## 4. 时间与时间戳

- **对外 JSON**：使用 ISO 8601 UTC，例如 `2026-03-28T12:00:00Z`
- **`google.protobuf.Timestamp`**：在 `textproto` 中写成消息形态，例如：

```textproto
updated_at {
  seconds: 1774699200
}
```

- 同一组示例中的时间应尽量自洽：
  - 创建时间 <= 更新时间
  - 过期时间 > 当前时间
  - 推荐、点击、转化链路时间应符合先后顺序

## 5. 枚举与字段名

- `textproto` 中枚举使用 **proto 枚举名**，不要写字符串
- JSON 示例中的字段名遵循公开契约，统一使用 `snake_case`
- 不要在示例中使用文档未定义、proto 未声明的字段
- 不要使用 `UNSPECIFIED` 作为业务有效示例值，除非文档明确允许

## 6. HTTP 成功响应约定

对外成功响应默认遵循：

```json
{
  "success": true,
  "code": 0,
  "message": "ok",
  "data": {},
  "meta": {
    "request_id": "req_xxx",
    "server_time_ms": 1774699800000
  }
}
```

补充约定：

- 若已有示例带 `trace_id`，同类复杂链路可继续保留
- token 发放类响应字段名使用 `expires_in`，不要写内部 proto 字段名 `expires_in_seconds`
- `message` 默认写 `ok`，不要在成功示例里混用其他成功文案

## 7. 示例粒度

- 每个 RPC / 路由至少保留 **1 组最小合法请求 + 1 组典型成功响应**
- 优先展示最小可执行路径，不要把示例写成全字段大全
- 如某 RPC 有明显双路径（例如匿名 / 登录、redirect / json body），可额外补第 2 组示例

## 8. 修改原则

- 若协议字段或共享样例语义变更，先更新本文件，再批量修正文档示例
- 若单个服务需要偏离本约定，应在该服务 `api.md` 示例附近写明原因
