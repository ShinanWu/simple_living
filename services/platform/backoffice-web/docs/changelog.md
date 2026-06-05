# backoffice-web 变更记录

记录本服务文档与对外行为/契约消费的重要变更。破坏性变更显式标注并列出受影响方。本服务不产出对外契约，仅消费 `gateway` backoffice API；契约变更以 `gateway/docs/backoffice-backend.md` 为准。

## 2026-06-04

- **文档补全到上线级**：重写 `docs/README.md` 为服务总览，覆盖目标、运营动作矩阵、职责/非职责、上下游依赖（引用 `gateway/docs/api.md` §13.6 与 `backoffice-backend.md`，不复制契约）、SLO（静态资源可用性、关键运营操作端到端成功率/延迟）、构建/运行/完整配置项表、登录鉴权与角色、部署与回滚验收、可观测性（前端错误分类、操作日志/审计入口、健康检查、`request_id`/`trace_id` 透传）、安全（最小权限、危险操作二次确认、token 处理现状与演进）。
- **明确不适用项**：前端 Web 无业务 PostgreSQL/proto/领域事件，DoD 的 `data-model.md` / `workflow.md` 不适用，权威数据模型指向 `gateway/docs/backoffice-backend.md` 与各域文档。
- **测试**：在 `test-plan.md` 增补「关键流程 E2E 上线门槛」（内容发布全生命周期、审核通过/拒绝、伙伴配置、审计查询）。
- **一致性修订**：部署说明合并至 `deploy/README.md`；backoffice 路由清单对齐 v3 完整能力，改为引用 `gateway/docs/backoffice-backend.md`。
- 无破坏性变更；运行时仍默认连真实 `gateway`，`FakeGatewayRepository` 仅作测试夹具。

## 历史（v3，2026-06-01）

- 内容管理扩展为完整生命周期（创建/编辑/详情/提交审核/发布/状态切换/回滚）与四种资源类型（`guide_card` / `editorial_content` / `topic` / `ranking_list`）。
- 错误处理细化为网络错误(`-1`)/接口未实现(`-2`)/HTTP 错误(`status`)/业务错误(`code`)四类。
- v3 端到端验收：gateway 接口冒烟与前端集成要点记录于 `test-plan.md` §7。
