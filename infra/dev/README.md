# 开发与类生产依赖（PostgreSQL / Redis / Kafka）

与 [`docs/architecture/README.md`](../../docs/architecture/README.md) §8 一致：关系型库为 **PostgreSQL**；缓存 **Redis**；消息 **Kafka**。

## 启动（本机或远程 Linux）

需已安装 [Docker](https://docs.docker.com/engine/install/) 与 Docker Compose v2。

```bash
cd /path/to/simple_living
docker compose -f infra/dev/docker-compose.yml up -d
docker compose -f infra/dev/docker-compose.yml ps
```

默认端口：

| 服务 | 端口 | 说明 |
|------|------|------|
| PostgreSQL | 5432 | 库 `simple_living`，用户/密码 `simple` / `simple` |
| Redis | 6379 | AOF 持久化卷 |
| Kafka（协议） | 9092 | 容器内为 **Redpanda**（与 Kafka 客户端协议兼容），单节点开发/联调 |

**远程编译/运行环境**可与本仓库约定一致：在同一台主机上 `docker compose up`，将 `user-domain` 等进程的 `-pg_conninfo` 指向 `host=127.0.0.1 port=5432 ...`，即可把该主机视为**类生产**依赖面。

## user-domain 连接串示例

与 `services/user-domain` 默认 gflags 对齐：

```text
host=127.0.0.1 port=5432 dbname=simple_living user=simple password=simple
```

## 停止与清理

```bash
docker compose -f infra/dev/docker-compose.yml down
# 同时删卷（清空数据）：
docker compose -f infra/dev/docker-compose.yml down -v
```

## 说明

- 镜像版本可按团队基线升级；变更时建议在 `docs/engineering-conventions.md` 或运维文档中留痕。
- 生产集群应使用托管或高可用部署，本 Compose **不**等同于生产 Kafka/PG 运维方案。
