# Gateway（统一入口 / BFF）

客户端与内部业务域之间的**唯一对外边界**：HTTPS + JSON 接入、鉴权、限流、JSON↔proto 映射、读路径聚合。不拥有导购内容、推荐、联盟、用户画像、跳转归因等业务真相。

- **终端契约**：[api.md](./api.md)
- **部署**：[deploy/README.md](./deploy/README.md)
- **全仓端口 / Bazel / 依赖拓扑**：[`services/README.md`](../README.md)

## 职责

| 类别 | 说明 |
|------|------|
| 统一入口 | 所有终端经 gateway 访问后端 |
| 鉴权 | 校验 `Authorization` / 访客会话头，注入下游 metadata（账号规则见 user-server） |
| 路由与映射 | HTTP → 内部 brpc；字段 `snake_case`，形状见 [api.md](./api.md) |
| BFF 聚合 | 首页/详情/跳转准备等页面路由，定义见 [api.md](./api.md) §13 |
| 信封与错误码 | 统一 `success`/`code`/`data`/`meta`，域错误码透传见 [api.md](./api.md) §8 |

## 非职责

不持久化业务主数据、不实现推荐/审核/转链/佣金等业务规则，不把内部 proto 直接暴露给客户端。

## 下游依赖

| 下游 | 文档 | 用途 |
|------|------|------|
| `user-server` | [api.md](../user-server/api.md) | 会话、资料、偏好、收藏、历史、反馈 |
| `recommendation-server` | [api.md](../recommendation-server/api.md) | C 端导购读、推荐 |
| `tracking-server` | [api.md](../tracking-server/api.md) | 跳转链、点击、归因 |
| `platform/backoffice-backend` | [api.md](../platform/api.md) | 运营写路径 `/api/v2/backoffice/*` |

运营后台 **HTTP 契约**（字段级）：[backoffice-gateway-api.md](../platform/backoffice-gateway-api.md)。

默认下游 brpc 地址（K8s DNS，本地可覆盖为 `127.0.0.1`）：

| 服务 | 端口 | flag |
|------|------|------|
| user-server | 9101 | `-user_server_addr` |
| recommendation-server | 9103 | `-recommendation_server_addr` |
| tracking-server | 9105 | `-tracking_server_addr` |
| backoffice-backend | 9110 | `-backoffice_backend_addr` |

## 本地开发

```bash
bazel build //services/gateway/...
bazel run //services/gateway:gateway_edge_server -- \
  -port=8080 \
  -user_server_addr=brpc://127.0.0.1:9101 \
  -recommendation_server_addr=brpc://127.0.0.1:9103 \
  -tracking_server_addr=brpc://127.0.0.1:9105 \
  -backoffice_backend_addr=brpc://127.0.0.1:9110
```

测试：`bazel test //services/gateway/...`。网关无状态，可选 `-redis_addr` 做限流/短缓存。

## SLO

| 指标 | 目标 |
|------|------|
| 对外可用性 | ≥ 99.9%（含合法降级响应） |
| 网关自身 P99 | ≤ 15ms（不含下游） |
| 首页 feed 端到端 P99 | ≤ 250ms |
| 单实例 QPS | ≤ 800（水平扩展） |

限流超限返回 `10005`（HTTP 429），阈值可通过 flags 调整（见 `gateway_edge_server_main.cpp`）。
