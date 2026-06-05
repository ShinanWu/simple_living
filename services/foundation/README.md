# Foundation 平台

商业化联调与生产的共享基础设施：PostgreSQL、Redis、Kafka。

## 组件

| 组件 | 部署脚本 | 默认来宾端口 | Mac host-forward |
|------|----------|--------------|------------------|
| PostgreSQL | `postgres/deploy/deploy_service.sh` | 5432 | 15432 |
| Redis | `redis/deploy/deploy_service.sh` | 6379 | 16379 |
| Kafka (KRaft) | `kafka/deploy/deploy_service.sh` | 9092 | 19092 |

业务 QEMU 来宾通过 `10.0.2.2:<host-forward-port>` 访问（见 `environments/local-qemu/nodes.env`）。

## QEMU 节点脚本

| 组件 | start/check/stop_nodes | 说明 |
|------|------------------------|------|
| PostgreSQL | 有 | 独立 `foundation` QEMU 来宾 |
| Redis / Kafka | 无 | 通过 `deploy_service.sh up` 在同一来宾上起 compose 容器 |

lab 默认只起一台 foundation 来宾（`postgres/deploy/start_nodes.sh`），Redis 与 Kafka 以容器形式叠在同一 VM。

## 常用命令

```bash
# 启动 foundation QEMU 节点
bash services/foundation/postgres/deploy/start_nodes.sh
bash services/foundation/postgres/deploy/check_nodes.sh

# 基础组件（在同一 foundation 来宾上）
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/foundation/redis/deploy/deploy_service.sh up
bash services/foundation/kafka/deploy/deploy_service.sh up

# 健康检查（本机经 host-forward）
bash services/foundation/scripts/health_check.sh

# 备份 / 恢复 PostgreSQL 卷
bash services/foundation/scripts/backup.sh
bash services/foundation/scripts/restore.sh <backup-archive.tar.gz>

# Schema 迁移（幂等）
bash services/foundation/scripts/run_migrations.sh
```

## Kafka Topic 规范

| Topic | 生产者 | 用途 |
|-------|--------|------|
| `content.published` | platform/backoffice-backend (outbox) | 内容发布，驱动推荐候选池 |
| `content.offlined` | platform/backoffice-backend | 下架，移除召回 |
| `tracking.link.created` | tracking-server | 链接创建审计 |
| `tracking.click.recorded` | tracking-server | 点击事件 |
| `tracking.conversion.ingested` | tracking-server | 转化回传 |
| `governance.visibility.changed` | platform/backoffice-backend | 可见性变更 |

保留策略（单节点 lab）：7 天；生产按合规与容量单独配置。

## Redis Key 规范

| 前缀 | 用途 | TTL |
|------|------|-----|
| `sl:session:` | 用户会话热读 | 与 refresh token 生命周期对齐 |
| `sl:ratelimit:` | gateway / 登录限流窗口 | 60s–3600s |
| `sl:rec:` | 推荐结果缓存 | 300s |
| `sl:idempotent:` | 写操作幂等键 | 86400s |

## Schema 迁移

- 全局注册表：`schema_migrations`（见 `migrations/0000_schema_registry.sql`）。
- 各服务在 `services/<service>/migrations/` 或 foundation `migrations/` 下追加版本化 SQL。
- 部署前执行 `scripts/run_migrations.sh`；服务 `ConnectAndInit` 仍保留幂等 DDL 作为双保险。
