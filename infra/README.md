# infra 目录边界

`infra/` 仅用于**在线环境管理与平台控制面**，不承载业务服务与业务依赖组件。

当前保留：

- `infra/k8s/`：集群公共基座（namespace/configmap/secret 模板）
- `infra/lab/`：实验节点管理与通用部署原语

不再包含：

- 数据库/缓存/消息等运行依赖服务（已迁移到 `services/`）
- 业务服务专属部署逻辑（已下沉到 `services/<service>/deploy/`）
