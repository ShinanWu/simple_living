# Foundation 部署（QEMU 节点 + 全组件）

本目录管理 **foundation 整层**：一台 QEMU 来宾及其上的 PostgreSQL、Redis、Kafka 容器。

| 脚本 | 作用 |
|------|------|
| `start_nodes.sh` | 启动 `foundation` QEMU 来宾 |
| `check_nodes.sh` | 检查 SSH 与 Mac 转发端口 |
| `stop_nodes.sh` | 停止 `foundation` QEMU 来宾 |
| `deploy_service.sh` | 一键 `up` / `down` / `down-v` / `health`（三个基础组件） |

端口唯一来源：`environments/local-qemu/lab-ports.env`（`5432` / `6379` / `9092`，Mac 转发与来宾同号）。

## 常用命令

```bash
bash services/foundation/deploy/start_nodes.sh
bash services/foundation/deploy/check_nodes.sh
bash services/foundation/deploy/deploy_service.sh up
bash services/foundation/scripts/health_check.sh
```

## 单组件容器

仅操作某一个基础组件时，使用各组件目录下的脚本（不启动 QEMU）：

```bash
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/foundation/redis/deploy/deploy_service.sh up
bash services/foundation/kafka/deploy/deploy_service.sh up
```
