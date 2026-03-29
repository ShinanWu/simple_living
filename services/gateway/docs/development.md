# Gateway — 开发与交付

本文说明本服务在仓库内的**实现、构建、本地运行、测试与合并前检查**约定，以及须对照的公共文档。

## 1. 本服务文档（均在当前 `docs/` 内）

| 顺序 | 文档 |
|------|------|
| 1 | [README.md](./README.md)（边界、职责、对外/对内协议） |
| 2 | [api.md](./api.md)（HTTPS+JSON 路由与 `data` 形状） |
| 3 | [workflow.md](./workflow.md)（阶段、metadata、超时与聚合） |
| 4 | [pages.md](./pages.md)（BFF 与页面聚合） |
| 5 | [data-model.md](./data-model.md)（网关侧上下文字段） |
| 6 | 变更记 [changelog.md](./changelog.md) |

## 2. 公共文档（仓库根 `docs/`）

| 文档 | 用途 |
|------|------|
| [docs/contracts/README.md](../../../docs/contracts/README.md) | 公共 JSON 语义与二层契约模型 |
| [docs/contracts/common-response.md](../../../docs/contracts/common-response.md) | 响应信封 |
| [docs/contracts/error-codes.md](../../../docs/contracts/error-codes.md) | 错误码 |
| [docs/engineering-conventions.md](../../../docs/engineering-conventions.md) | Bazel、全端口表、跨服务依赖矩阵、测试与 CI（细节以该文为准） |

架构与依赖拓扑：[docs/architecture/README.md](../../../docs/architecture/README.md)。

## 3. 本仓库内实现边界（摘要）

- **终端**：只使用 **HTTPS + JSON**；`gateway/proto` 中 Edge service 为 **逻辑路由边界**，不是客户端直连的 wire API。完整说明见 [README.md §6](./README.md) 与 `docs/engineering-conventions.md` §3。
- **下游**：经 **brpc** 调用各业务域；不得把业务规则下沉到错误映射之外的新增逻辑而不更新文档。

## 3.1 状态与基础设施（顶层设计）

`gateway` **不承载业务权威状态**；鉴权、限流、聚合所需的外部能力按 [`docs/architecture/README.md`](../../../docs/architecture/README.md) §8：**Redis**（如限流计数/短期缓存）、**MySQL / PostgreSQL**（若网关侧仅存审计/配置类数据，须在 `data-model.md` 写明）、**Kafka**（异步旁路，若有）。默认实现以**无状态多实例**为准。细则见 `docs/engineering-conventions.md` §4.1。

## 4. Bazel 与源码位置

| 项 | 值 |
|----|-----|
| 包路径 | `//services/gateway` |
| 主二进制 | `//services/gateway:gateway_edge_server`（名称以 `BUILD.bazel` 为准） |
| Proto | `proto/*.proto` |
| 实现 | `src/` |
| 测试 | `tests/` |

构建：`bazel build //services/gateway/...`

## 5. 运行与联调（本服务）

| 项 | 约定 |
|----|------|
| 默认对外端口 | `8080`（`-port` 覆盖） |
| 下游 | 为每个可能调用的域配置 `-*_domain_addr`，形式 `brpc://127.0.0.1:<端口>`；端口全表见 `docs/engineering-conventions.md` §4 |

示例（先启动各域 brpc，再启 gateway）：

```bash
bazel run //services/gateway:gateway_edge_server -- \
  -port=8080 \
  -user_domain_addr=brpc://127.0.0.1:9101 \
  -content_domain_addr=brpc://127.0.0.1:9102 \
  -recommendation_domain_addr=brpc://127.0.0.1:9103 \
  -affiliate_domain_addr=brpc://127.0.0.1:9104 \
  -tracking_domain_addr=brpc://127.0.0.1:9105 \
  -governance_domain_addr=brpc://127.0.0.1:9106
```

## 6. 允许的 Bazel 依赖（proto / cc_proto）

可依赖各业务域已导出的 `proto_library` / `cc_proto_library`，**不拷贝**他域 `.proto`。方向表见 `docs/engineering-conventions.md` §2.2。

## 7. 测试

- 目录：`tests/`；目标命名与 `cc_test` 约定见 `docs/engineering-conventions.md` §5。
- **最低期望**：至少一条 HTTP 成功路径 + 一条典型错误响应；校验顶层信封与 `docs/contracts` 一致。

```bash
bazel test //services/gateway/...
```

## 8. 合并前检查清单

- [ ] `docs/`（含 `api.md`）与实现及 `docs/contracts/` 一致；`changelog.md` 已更新
- [ ] 无状态与外部依赖（**Redis** / 可选库表）与 [`docs/architecture/README.md`](../../../docs/architecture/README.md) §8、`docs/engineering-conventions.md` §4.1 一致
- [ ] `bazel build //services/gateway/...` 通过
- [ ] `bazel test //services/gateway/...` 通过（或 PR 说明暂缓与跟踪项）
- [ ] 交付含 **HTTPS+JSON 接入**（或 changelog 已说明分阶段计划）
- [ ] 默认端口与下游 flags 与 `docs/engineering-conventions.md` §4 一致（若变更则同步该文与各域 `development.md`）

## 9. 协调范围

若修改对外 JSON 或错误语义：必改 `docs/contracts/`（如适用）、本目录 `api.md`、`changelog.md`，并通知相关域文档 owner。跨域 proto 变更由提供方服务合并，gateway 只调 Bazel 依赖与映射代码。
