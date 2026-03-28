# governance-domain — 变更日志

本文档记录 **本服务域文档与契约设计** 的变更，便于并行开发时对齐版本。服务实现发布说明可在此追加或指向发布系统。

## [未发布] - 文档初稿

### 新增

- 初始化 `services/governance-domain/docs/` 文档集：`README.md`、`api.md`、`pages.md`、`data-model.md`、`workflow.md`、`changelog.md`。
- 明确职责：内容审核、商业合作标识、发布/下架、运营配置、风险标记、策略执行。
- 明确边界：不包含推荐排序与联盟技术对接；与 content / recommendation / affiliate / tracking 的协作方式草案。

### 待后续版本补齐

- 与 `docs/contracts/` 中具体文件（error-codes、guide-card、recommendation 等）的字段级交叉引用。
- API 路径、gRPC 服务名、Kafka topic 的最终命名规范。
- `visibility:evaluate` 的 SLA 与缓存/fail-open|closed 的最终产品决策。

---

## 版本说明（建议）

后续可采用 **文档版本号**（如 `gov-docs-0.2.0`）在 PR 标题或本文件标题中标注；与二进制服务版本可分离，但需在联调群中同步。
