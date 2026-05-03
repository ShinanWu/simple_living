# backoffice-web 文档索引

## 目标

本目录作为 `services/platform/backoffice-web` 的单一事实来源，定义运营后台 Web 的页面模型、接口约束、状态机与验收口径。

## 文档清单

- `detail-design.md`：页面信息架构、交互状态、权限边界、审计展示规则
- `test-plan.md`：分层测试策略、回归清单、QEMU 联调与上线前检查

## 协作边界

- 运营后台只消费 `gateway` 暴露的 backoffice JSON 契约。
- 不直接依赖域服务实现目录或内部 proto。
- 网关与域模型变更时，先更新契约文档，再更新本目录文档与实现。
