# Foundation 平台

商业化联调与生产的共享基础设施：PostgreSQL、Redis、Kafka。

## 目录结构

```
services/foundation/
  deploy/                 # QEMU 节点 + 三组件一键部署（入口在这里）
  postgres/deploy/        # 仅 PostgreSQL 容器
  redis/deploy/           # 仅 Redis 容器
  kafka/deploy/           # 仅 Kafka 容器
  scripts/                # 健康检查、迁移、备份
  migrations/             # 全局 schema 注册表
```

端口唯一来源：`environments/local-qemu/lab-ports.env`（`5432` / `6379` / `9092`；Mac 转发与来宾同号）。

## 组件

| 组件 | 容器部署脚本 | 端口 |
|------|--------------|------|
| PostgreSQL | `postgres/deploy/deploy_service.sh` | `5432` |
| Redis | `redis/deploy/deploy_service.sh` | `6379` |
| Kafka (KRaft) | `kafka/deploy/deploy_service.sh` | `9092` |

业务 QEMU 来宾通过 `10.0.2.2:<端口>` 访问（见 `environments/local-qemu/nodes.env`）。

## 常用命令

```bash
# QEMU 节点（整层）
bash services/foundation/deploy/start_nodes.sh
bash services/foundation/deploy/check_nodes.sh
bash services/foundation/deploy/stop_nodes.sh

# 三个基础组件（同一 foundation 来宾上）
bash services/foundation/deploy/deploy_service.sh up
bash services/foundation/deploy/deploy_service.sh health

# 单组件
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/foundation/redis/deploy/deploy_service.sh up
bash services/foundation/kafka/deploy/deploy_service.sh up

# 运维
bash services/foundation/scripts/health_check.sh
bash services/foundation/scripts/run_migrations.sh
bash services/foundation/scripts/backup.sh
bash services/foundation/scripts/restore.sh <backup-archive.tar.gz>
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
