# Gateway 文档变更记录

本文档记录 **gateway 服务契约与文档** 的变更，便于多端与多域并行开发时对齐版本。

## 版本说明

- **文档版本**与运行中的 gateway 二进制版本可不同步，但对外承诺以 **已发布的文档 + contracts** 为准。
- 破坏性变更必须递增对外 API 版本路径（如 `/api/v2`）或经过书面迁移期（在本 changelog 说明）。

---

## [1.0.0] - 2026-03-28

### 新增

- 初始化 `services/gateway/docs/` 文档集：`README.md`、`api.md`、`pages.md`、`data-model.md`、`workflow.md`、`changelog.md`。
- 明确 gateway **边界层** 定位：统一对外入口、鉴权入口、路由、限流、JSON ↔ proto 翻译、读路径 BFF 聚合；**不**作为业务域数据与规则所有者。
- 对齐 [docs/architecture/README.md](../../../docs/architecture/README.md) 与 [docs/contracts/README.md](../../../docs/contracts/README.md)：客户端 JSON（`snake_case`）、内部 proto、公共契约引用关系。
- `api.md`：路径版本建议、HTTP 方法、请求头、响应信封、错误与 HTTP 状态码策略、JSON/proto 职责划分。
- `pages.md`：BFF 原则、典型页面占位表、聚合失败策略与分页约定。
- `data-model.md`：对外 canonical 引用 contracts；gateway 侧请求上下文逻辑字段；无业务主数据模型。
- `workflow.md`：端到端处理阶段、metadata 传播、聚合超时与部分失败、观测要点。

### 兼容性与后续工作

- 具体路由表、RPC 方法全名、错误映射表需在 proto 与实现落地后回填 `api.md` / `pages.md` 附录。
- 若限流统一采用「仅 200 + 信封」或「429 + 信封」，需在下一文档版本锁定一种默认策略并更新 `api.md`。
