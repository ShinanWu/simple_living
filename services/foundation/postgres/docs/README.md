# postgres 运行服务说明

`postgres` 是全域**关系型权威存储**（system of record），为各业务域提供持久化能力。正式本地联调运行在单独的 `foundation` QEMU 节点，避免业务服务节点承载数据库进程。

## 1. 定位与边界

- **负责**：所有业务域的关系型持久化权威数据；schema 迁移注册与执行入口；备份与恢复。
- **不负责**：业务规则与 schema 定义本身——各表结构、索引、迁移 SQL 由对应服务在 `services/<service>/docs/data-model.md` 与 `services/<service>/migrations/` 拥有；本服务只提供运行实例与统一迁移/备份机制。
- **权威性约束**（见 `services/README.md` §4.3）：PostgreSQL 是唯一跨实例权威；Redis 仅缓存、Kafka 仅异步，二者均不得作为权威来源。
- **上游消费方**：所有持有 `pg_conninfo` 的业务域服务（user/content/recommendation/affiliate/tracking/governance）。

## 2. 连接与 `pg_conninfo`

各域服务通过连接池 + 仓储层访问，连接参数来自 `environments/local-qemu/nodes.env`（见 `environments/local-qemu/nodes.example.env`）：

| 维度 | 默认值 | 说明 |
|------|--------|------|
| 来宾端口 | `5432` | 容器内 PostgreSQL 监听端口 |
| Mac host-forward | `15432` | 本机经 QEMU 转发访问 |
| 业务来宾访问地址 | `10.0.2.2:15432` | 其他 QEMU 业务节点经 host-forward 访问 |
| Database | `simple_living` | 单库多 schema/表，按域归属 |
| 账号 | `simple`（lab 默认开发账号，非生产密钥） | 见下「账号与密钥来源」 |
| 镜像 | `postgres:16-alpine` | `deploy/deploy_service.sh` / `deploy/docker-compose.yml` |
| 数据卷 | `simple_living_pg` → `/var/lib/postgresql/data` | 持久化目录 |

各域 `pg_conninfo` 形如（口令不写入文档/Git，由环境注入）：

```
host=10.0.2.2 port=15432 dbname=simple_living user=simple password=$POSTGRES_PASSWORD
```

业务服务通过 `-pg_conninfo` flag 或等价 env 接收完整连接串（见各域 `development.md` 配置项表）。

## 3. 账号与密钥来源（不写明文）

- lab 默认开发账号 `simple` 与库 `simple_living` 是**非密标识符**，由 `deploy/deploy_service.sh` 与 `deploy/docker-compose.yml` 设置，仅限本地/内网联调。
- **口令不写入文档与 Git**：lab 口令在部署脚本/compose 内设置，仅作本地非密占位；生产口令由密钥管理（K8s Secret / 系统钥匙串 / 凭据管理器）注入到 `environments/local-qemu/nodes.env`（已 gitignore）的 `POSTGRES_PASSWORD`，再被脚本读取。
- 轮换口令时只更新密钥源与 `nodes.env`，不改动文档。

## 4. 部署 / 回滚 / 排障 Runbook

### 4.1 准备节点

```bash
bash services/foundation/postgres/deploy/start_nodes.sh   # 启动 foundation QEMU 节点
bash services/foundation/postgres/deploy/check_nodes.sh   # 校验 ssh 与转发端口
```

### 4.2 部署 / 停止

```bash
bash services/foundation/postgres/deploy/deploy_service.sh up      # 拉起容器（restart unless-stopped）
bash services/foundation/postgres/deploy/deploy_service.sh health  # 复用统一健康检查
bash services/foundation/postgres/deploy/deploy_service.sh down    # 删除容器，保留数据卷
bash services/foundation/postgres/deploy/deploy_service.sh down-v  # 删除容器 + 数据卷（销毁数据，谨慎）
```

可选诊断（本机 compose 形态）：

```bash
docker compose -f services/foundation/postgres/deploy/docker-compose.yml ps
```

### 4.3 回滚

- 镜像 pin 在 `postgres:16-alpine`；如需回退到上一个镜像 tag，修改 `deploy_service.sh` / `docker-compose.yml` 的镜像版本后重新 `up`。数据卷 `simple_living_pg` 在 `down`（非 `down-v`）时保留，故镜像回滚不丢数据。
- **迁移回滚**：v1 采用「只前滚」策略，迁移文件向后兼容、不写破坏性变更；如确需回退数据，使用 §5 的备份恢复到迁移前快照（范围外的自动 down-migration 不在 v1）。
- 部署失败回退：`down` 后修正参数重新 `up`；数据卷未销毁则状态保持。

### 4.4 排障

| 现象 | 排查 |
|------|------|
| 业务服务连不上 | 先 `check_nodes.sh` 看 `15432` 是否 reachable；再 `health` 子命令；确认 `nodes.env` 的 `POSTGRES_*` |
| 端口未监听 | 容器是否 Running（`docker/podman ps`）；host-forward 端口是否被占用 |
| 迁移失败 | 见 §6；`run_migrations.sh` 以 `ON_ERROR_STOP=1` 中断，修正 SQL 后重跑（幂等） |
| 数据疑似丢失 | 检查是否误用 `down-v`；用 §5 恢复最近备份 |

日志：容器标准输出（`docker/podman logs simple-living-postgres`）；QEMU 节点串口日志见 `environments/local-qemu/vms/foundation.log`。

## 5. 备份 / 恢复 / 数据保留

- **备份**：`bash services/foundation/scripts/backup.sh`
  - 经 SSH 在 foundation 节点对容器执行 `pg_dump -U simple simple_living`，gzip 后写入仓库根 `.foundation-backups/foundation_pg_<时间戳>.tar.gz`。
- **恢复**：`bash services/foundation/scripts/restore.sh <backup.sql.gz>`
  - `gunzip` 后管道到容器内 `psql -d simple_living`。恢复前建议先停业务写入。
- **数据保留**：本地 lab 备份保留策略由运维按磁盘容量手动清理 `.foundation-backups/`（建议保留最近 7 份）；生产保留周期与加密归档按合规单独配置（v1 范围外的自动归档/异地副本）。
- **PITR（时间点恢复）**：v1 **范围外**。v1 仅提供 `pg_dump` 逻辑备份的「全量快照恢复」；WAL 归档 + PITR 待多副本/生产阶段引入。

## 6. 迁移入口与命名规范

统一入口（幂等、可重复执行）：

```bash
bash services/foundation/scripts/run_migrations.sh
```

执行顺序与机制：

1. 先建立全局注册表 `schema_migrations`（`migrations/0000_schema_registry.sql`，`version TEXT PRIMARY KEY`）。
2. 按文件名顺序应用 `services/foundation/migrations/*.sql`（跳过 `0000_*`）。
3. 再遍历 `services/*/migrations/*.sql` 与 `services/platform/*/migrations/*.sql`。
4. 每个文件以 `basename` 作为 `version`，已记录则 `SKIP`，否则 `APPLY` 并写入 `schema_migrations`（双保险：服务 `ConnectAndInit` 仍保留幂等 DDL）。

**命名规范**：`NNNN_<domain>_<change>.sql`

- `NNNN`：四位递增序号（`0000` 保留给注册表）。
- `<domain>`：归属域，如 `content`、`tracking`、`governance`。
- `<change>`：变更简述，如 `0007_content_add_topic_index.sql`。
- foundation 自身的跨域基础 SQL 用 `<domain>=foundation`。

约束：迁移**向后兼容、只前滚**；不可改写已应用文件（应新增版本）；破坏性变更需在对应域 `changelog.md` 标注并列出受影响消费方。

## 7. 容量、连接数与 HA

- **容量假设（v1 单实例 lab）**：单库承载全部域数据，初期数据量在 GiB 级；节点默认 `4096MB` 内存、`80G` 磁盘（见 `start_nodes.sh`）。
- **连接数**：业务侧用连接池约束并发；默认未调高 `max_connections`（沿用镜像默认 100），各域连接池上限之和应留余量。超限时调大 `max_connections` 或收紧池大小。
- **HA / 持久化策略**：**v1 单实例 + 数据卷持久化 + 逻辑备份恢复**；多副本/流复制/自动故障转移**范围外**，待生产阶段以 K8s StatefulSet + 流复制引入。

## 8. 安全与网络边界

- **网络边界**：仅内网/隧道可达——foundation 节点 PostgreSQL 端口只经 QEMU host-forward 暴露在本机回环与业务来宾的 `10.0.2.2`，**不对公网开放**。
- **认证**：口令认证；账号/口令来源见 §3，密钥不写明文。
- **传输**：lab 内网为明文连接（可信网络内）；生产跨网段须启用 TLS（范围外，随生产网络方案引入）。
- **优雅退出**：容器以 `restart unless-stopped` 运行；`down` 走容器停止路径，PostgreSQL 正常关闭后再删除容器，避免强杀导致的脏关闭。

## 9. 健康检查

```bash
bash services/foundation/scripts/health_check.sh
# 或：bash services/foundation/postgres/deploy/deploy_service.sh health
```

优先用 `pg_isready -h <host> -p 15432 -U simple -d simple_living`，无客户端时回退 `nc -z` 端口探测。

## 10. SLO（v1 lab 目标）

| 指标 | 目标 |
|------|------|
| 可用性 | 联调时段单实例 best-effort，无多副本保障；以备份恢复兜底数据安全 |
| 连接建立延迟 | P99 < 50ms（内网/本机转发） |
| 简单点查 | P99 < 20ms（命中索引） |
| 恢复目标 | RPO = 上次 `backup.sh` 时点；RTO = 一次 `restore.sh` 耗时（分钟级） |

> 生产级可用性 SLA（如 99.9%）与多副本目标在 HA 引入后单独定义，v1 明确范围外。
