# redis 运行服务说明

`redis` 是全域**缓存与轻量状态**运行服务，为热读、限流与短时窗口提供支撑。正式本地联调运行在 `foundation` QEMU 节点。

## 1. 定位与边界

- **负责**：缓存层与短时易失状态——会话热读、热点内容、限流计数、幂等窗口、可见性快照。
- **不负责**：**不得作为跨实例权威**（见 `services/README.md` §4.3）。所有权威数据在 PostgreSQL；Redis 内容可随时丢失并从权威源重建。
- **上游消费方**：gateway（会话/限流）、recommendation（结果缓存）、各域写路径（幂等键）。

## 2. 缓存用途与 Key 前缀规范

统一前缀 `sl:`，按用途二级命名 `sl:<用途>:<标识>`：

| 前缀 | 用途 | TTL 约定 | 失效/重建 |
|------|------|----------|-----------|
| `sl:session:` | 用户会话热读 | 与 refresh token 生命周期对齐 | 登出/令牌轮换时显式删除；缺失则回源 user-server |
| `sl:ratelimit:` | gateway / 登录限流窗口计数 | 60s–3600s（按窗口） | 到期自动失效，无需重建 |
| `sl:rec:` | 推荐结果缓存 | 300s | 到期重算；内容/可见性变更可主动失效 |
| `sl:idempotent:` | 写操作幂等键窗口 | 86400s | 窗口内重复请求命中即拒绝重复副作用 |
| `sl:visibility:` | 治理可见性快照 | ≤300s | governance 可见性变更时主动失效 |
| `sl:hot:` | 热点内容缓存 | 60s–600s | 到期重建；可被发布事件主动失效 |

规范要点：

- **必须带前缀**，禁止裸 key；新用途新增前缀并在此表登记。
- **必须设 TTL**：除限流窗口外，缓存类 key 一律设过期，避免内存无界增长。
- **失效事件**：权威数据变更时由写方主动 `DEL`/`EXPIRE` 对应前缀 key（如发布、可见性变更），不依赖被动过期保证一致性强度。
- **一致性策略**：缓存为旁路（cache-aside）；缺失或失效后回源 PostgreSQL 重建。

## 3. 配置项

| 维度 | 默认值 | 说明 |
|------|--------|------|
| 端口 | `6379` | 来宾、容器、Mac 转发同号（`lab-ports.env`） |
| 业务来宾访问地址 | `10.0.2.2:6379` | 其他 QEMU 业务节点经 slirp 网关访问 |
| 镜像 | `redis:7-alpine` | `deploy/deploy_service.sh` / `deploy/docker-compose.yml` |
| 持久化 | AOF（`--appendonly yes`） | `appendfsync everysec` 默认 |
| 数据卷 | `simple_living_redis` → `/data` | AOF 文件目录 |

业务服务通过 `-redis_addr`（或等价 env）接收地址（见各域 `development.md`）。

## 4. 账号与密钥来源（不写明文）

- lab 默认**无 `requirepass`**（仅内网/本机转发，未公网暴露）。
- 生产须启用 `requirepass`：口令由密钥管理注入 `environments/local-qemu/nodes.env`（已 gitignore）的 `REDIS_PASSWORD`，由部署脚本读取；**不写入文档/Git**。

## 5. 持久化、内存与淘汰策略

- **持久化**：启用 AOF（`--appendonly yes`），重启可从 AOF 重放。因 Redis 仅缓存，AOF 主要用于减少冷启动回源风暴，**丢失 AOF 不影响权威数据**。
- **内存上限**：lab 默认未设 `maxmemory`（受节点 4096MB 内存约束）；生产须设 `maxmemory` 上限。
- **淘汰策略**：缓存语义下推荐 `maxmemory-policy allkeys-lru`（或 `volatile-lru`，因关键 key 均设 TTL）；lab 沿用默认 `noeviction`，生产部署时按上表显式设定。
- v1 单实例：无主从/集群分片，**多副本范围外**，以「失效即回源」保证可恢复性。

## 6. 部署 / 回滚 / 排障 Runbook

### 6.1 部署 / 停止

```bash
bash services/foundation/redis/deploy/deploy_service.sh up      # 拉起容器（restart unless-stopped, appendonly）
bash services/foundation/redis/deploy/deploy_service.sh health  # 复用统一健康检查
bash services/foundation/redis/deploy/deploy_service.sh down    # 删除容器，保留数据卷
bash services/foundation/redis/deploy/deploy_service.sh down-v  # 删除容器 + 数据卷（清空 AOF）
```

可选诊断：

```bash
docker compose -f services/foundation/redis/deploy/docker-compose.yml ps
```

### 6.2 回滚

- 镜像 pin 在 `redis:7-alpine`；回退改镜像 tag 后重新 `up`。数据卷在 `down`（非 `down-v`）保留。
- 因 Redis 非权威，回滚/清空缓存安全：清空后各前缀按 §2 回源重建，仅产生短时回源压力。

### 6.3 排障

| 现象 | 排查 |
|------|------|
| 连不上 | `health` 子命令；`redis-cli -h <host> -p 6379 ping` 应返回 `PONG`；查端口占用 |
| 内存增长异常 | 检查是否有未设 TTL 的 key（裸 key/违反 §2）；`redis-cli --bigkeys` 抽查 |
| 命中率低 | 核对 TTL 是否过短、失效事件是否过度删除 |

日志：`docker/podman logs simple-living-redis`。

## 7. 健康检查

```bash
bash services/foundation/scripts/health_check.sh
# 或：bash services/foundation/redis/deploy/deploy_service.sh health
```

优先 `redis-cli -h <host> -p 6379 ping`（期望 `PONG`），无客户端时回退 `nc -z` 端口探测。

## 8. 安全与网络边界

- **网络边界**：仅内网/隧道可达——端口只经 QEMU host-forward 暴露在本机回环与业务来宾的 `10.0.2.2`，**不对公网开放**。
- **认证**：lab 无口令（可信内网）；生产启用 `requirepass`，密钥来源见 §4。
- **优雅退出**：`restart unless-stopped`；`down` 走正常停止，AOF 落盘后再删容器。

## 9. SLO（v1 lab 目标）

| 指标 | 目标 |
|------|------|
| 可用性 | 联调时段单实例 best-effort；缓存丢失不影响权威，回源可恢复 |
| 命令延迟 | P99 < 5ms（内网/本机转发，单 key 操作） |
| 缓存命中率 | 热点读 > 80%（用途相关，非硬指标） |

> 生产级可用性 SLA 与多副本目标在 HA 引入后单独定义，v1 明确范围外。
