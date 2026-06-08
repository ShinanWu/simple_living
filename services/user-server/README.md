# user-server

通用用户域服务：账号、会话与令牌、资料、偏好、收藏、历史、反馈、隐私同意、个性化信号引用。对外经 **gateway** 暴露 HTTPS+JSON；本服务提供内部 brpc RPC。

- **RPC 契约**：[api.md](./api.md)
- **部署**：[deploy/README.md](./deploy/README.md)
- **终端 JSON 映射**：[`services/gateway/api.md`](../gateway/api.md)

## 职责

| 能力 | 说明 |
|------|------|
| 账号与认证 | 注册标识、OTP/OAuth 等登录绑定 |
| 会话与令牌 | 签发/刷新/撤销 access·refresh；`IntrospectAccessToken` 供 gateway 鉴权 |
| 资料与偏好 | 展示信息、主题兴趣；偏好带乐观锁（冲突码 `10007`） |
| 收藏 / 历史 | 通用 `content_ref`；历史为用户可见摘要，非原始点击流 |
| 反馈 / 同意 | 结构化反馈；个性化/分析/营销开关 |
| 信号引用 | `signal_bundle_ref` 供推荐等下游消费（受同意门控） |

## 非职责

推荐排序、内容正文、点击归因、治理裁决、对外 HTTP 路径与 JSON 字段名（见 gateway）。

## 依赖

| 方向 | 服务 | 文档 |
|------|------|------|
| 消费方 | `gateway` | [api.md](../gateway/api.md) |
| 消费方 | `recommendation-server` | 同意与 signal bundle |
| 协作 | `tracking-server` | 事件可流入历史/分析；本域不存完整原始事件 |

数据：PostgreSQL 权威；Redis 可选（会话/内省缓存）。迁移见 `services/foundation/migrations/NNNN_user_*.sql`。

## 本地开发

```bash
bazel build //services/user-server/...
bazel test //services/user-server/...

bazel run //services/user-server:user_server -- \
  -port=9101 \
  -pg_conninfo="host=127.0.0.1 port=5432 dbname=user user=user password=$PG_PWD"
```

| flag | 必填 | 说明 |
|------|------|------|
| `-port` | 否 | 默认 `9101` |
| `-pg_conninfo` | 是 | PostgreSQL 连接串 |
| `-redis_addr` | 否 | 会话/内省缓存；空则直查库 |
| `-kafka_brokers` | 否 | 域事件 outbox（若启用） |

机密：`USER_TOKEN_HMAC_KEY`、`USER_PII_ENC_KEY` 等经部署环境注入，见 [deploy/README.md](./deploy/README.md)。

## SLO

| 指标 | 目标 |
|------|------|
| 鉴权可用性 | ≥ 99.95% |
| 内省 P99（缓存命中） | ≤ 20ms |
| 用户态读 P99 | ≤ 80ms |
