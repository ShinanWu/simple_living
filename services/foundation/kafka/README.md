# kafka 运行服务说明

`kafka` 是全域**异步事件总线**，用于域间解耦、回传与事件驱动。`deploy/deploy_service.sh` 采用 Apache Kafka 3.8.0 **KRaft 单节点**（无 ZooKeeper）；`deploy/docker-compose.yml` 提供本机单主机的 Redpanda（Kafka 协议兼容）兜底形态。正式本地联调运行在 `foundation` QEMU 节点。

## 1. 定位与边界

- **负责**：跨域异步事件投递（发布、点击、转化、可见性变更等）；事务性 outbox 中继。
- **不负责**：**不持有业务权威**（见 `services/README.md` §4.3）。权威在 PostgreSQL；Kafka 仅承载已提交事实的异步广播，消费方据事件更新各自读模型/缓存。
- **生产者**：content / tracking / governance 域（经 outbox）。**消费者**：recommendation（候选刷新）、分析/审计等。

## 2. Topic 命名与规范

命名规范：`<domain>.<event>`（点分，小写，事件用过去式名词短语）。

| Topic | 生产者 | 用途 |
|-------|--------|------|
| `content.published` | platform/backoffice-backend (outbox) | 内容发布，驱动推荐候选池 |
| `content.offlined` | platform/backoffice-backend | 下架，移除召回 |
| `tracking.link.created` | tracking-server | 链接创建审计 |
| `tracking.click.recorded` | tracking-server | 点击事件 |
| `tracking.conversion.ingested` | tracking-server | 转化回传 |
| `governance.visibility.changed` | platform/backoffice-backend | 可见性变更 |

规范要点：

- 新增 topic 必须为 `<domain>.<event>` 形式并在此表登记；topic 由产域拥有，消息 payload 字段契约在产域 `workflow.md`（领域事件契约段）定义，消费方据此对接，不靠口头。
- **消息 key**：用领域唯一键（如 `event_id`、`guide_card_id`）作为 key，保证同 key 有序与分区亲和。
- **分区策略（单节点 lab）**：默认 1 分区/topic；生产按吞吐与并行消费需求提分区数（提分区数不可回退，规划时预留）。
- **保留策略**：单节点 lab 默认 7 天；生产按合规与容量单独配置（如转化类长保留、点击类短保留）。

## 3. 消费者组规范

- 消费者组名 `<consumer-domain>.<purpose>`，如 `recommendation.candidate-refresh`。
- 每个独立消费目的一个组；同组多实例水平扩缩消费分区。
- **位移提交**：处理成功后再提交位移（at-least-once 语义，见 §4）；不在处理前提前提交。

## 4. 事务性 Outbox 与投递语义

事件由生产侧先写 PostgreSQL `tracking_event_outbox`（与业务写同事务，保证「业务成功 ⇔ 事件待发」），再由中继投递到 Kafka：

```bash
bash services/foundation/kafka/scripts/outbox_relay.sh
# 或：bash services/foundation/kafka/deploy/deploy_service.sh relay
```

中继机制（`outbox_relay.sh`）：

1. 查 `tracking_event_outbox` 中 `published = FALSE` 的批（默认 `BATCH=50`，按 `event_id` 排序）。
2. 以 `event_id` 为 Kafka 消息 key，`payload` 为消息体，投递到行内 `topic`（优先 `kcat`，回退 `kafka-console-producer.sh`）。
3. 投递成功后置 `published = TRUE`。

**投递语义：at-least-once**

- 中继在「投递成功」与「标记 published」之间若崩溃，重启后会重发同一 `event_id`（可能重复）。
- 因此**消费方必须按 `event_id` 幂等**：以 `event_id` 去重（如消费侧维护已处理 `event_id` 集合 / 唯一约束），重复投递不得产生重复副作用。
- 不保证全局严格有序；同 key（同 `event_id` 或同领域键）在单分区内有序。

## 5. 配置项

| 维度 | 默认值 | 说明 |
|------|--------|------|
| 来宾 bootstrap | `127.0.0.1:9092` | 容器内监听 |
| 端口 | `9092` | 来宾、容器、Mac 转发同号（`lab-ports.env`） |
| advertised listener | `10.0.2.2:9092` | 业务来宾经 slirp 网关连接地址 |
| 镜像（部署形态） | `apache/kafka:3.8.0`（KRaft） | `deploy/deploy_service.sh` |
| 镜像（compose 兜底） | Redpanda `v24.2.11` | `deploy/docker-compose.yml`（本机单主机，advertise `127.0.0.1:9092`） |
| 数据卷 | `simple_living_kafka` | 日志段持久化目录 |
| 复制因子 | 1（单节点） | offsets / txn-state 均为 1 |

业务服务通过 `-kafka_bootstrap`（或 env `KAFKA_BOOTSTRAP_SERVERS`，默认 `10.0.2.2:9092`）接收地址。

> 注意：deploy 形态（Apache Kafka）advertised 为 `10.0.2.2:9092`，compose 兜底形态（Redpanda）advertised 为 `127.0.0.1:9092`；两者不同时使用，按运行形态取对应地址。

## 6. 账号与密钥来源（不写明文）

- lab 默认 `PLAINTEXT` 监听、无 SASL（仅内网/本机转发，未公网暴露）。
- 生产须启用 SASL/TLS：凭据由密钥管理注入 `environments/local-qemu/nodes.env`（已 gitignore），**不写入文档/Git**。

## 7. 部署 / 回滚 / 排障 Runbook

### 7.1 部署 / 停止

```bash
bash services/foundation/kafka/deploy/deploy_service.sh up      # KRaft 单节点（restart unless-stopped）
bash services/foundation/kafka/deploy/deploy_service.sh health  # 复用统一健康检查（端口探测）
bash services/foundation/kafka/deploy/deploy_service.sh relay    # 触发一次 outbox 中继
bash services/foundation/kafka/deploy/deploy_service.sh down     # 删除容器，保留数据卷
bash services/foundation/kafka/deploy/deploy_service.sh down-v   # 删除容器 + 数据卷（清空日志段与位移）
```

可选诊断（Redpanda 兜底形态）：

```bash
docker compose -f services/foundation/kafka/deploy/docker-compose.yml ps
```

### 7.2 回滚

- 镜像 pin（Apache Kafka `3.8.0` / Redpanda `v24.2.11`）；回退改镜像 tag 后重新 `up`。数据卷在 `down`（非 `down-v`）保留，位移与日志段不丢。
- **不要轻易 `down-v`**：会丢失未消费消息与消费位移；优先 `down` 保留卷。
- 事件层回滚以 outbox 为准：未 `published` 的事件在中继恢复后补投；已投递的依赖消费方 `event_id` 幂等避免重复。

### 7.3 排障

| 现象 | 排查 |
|------|------|
| 生产/消费连不上 | `health` 子命令探测 `9092`；核对运行形态对应的 advertised 地址（§5） |
| 消费者收不到 | 确认 topic 已创建、消费者组与位移；查 `docker/podman logs simple-living-kafka` |
| outbox 不投递 | 中继需 `psql` 与 `kcat`/`kafka-console-producer.sh`；缺工具会 WARN 并直接标记 published（见脚本），需补齐客户端 |
| 积压增长 | 见 §8 容量与积压监控；扩消费实例或提分区 |

## 8. 容量与积压监控

- **容量假设（单节点 lab）**：低吞吐事件流（发布/点击/转化），单分区可承载；节点 4096MB 内存约束下控制保留期与消息体积。
- **积压监控**：以「消费组 lag = 分区 log-end-offset − 已提交位移」衡量；lab 阶段用 `kafka-consumer-groups.sh --describe` / Redpanda `rpk group describe` 人工抽查；持续增长说明消费侧落后，需扩实例或排查消费异常。
- **outbox 积压**：监控 `tracking_event_outbox` 中 `published = FALSE` 行数；持续增长说明中继未运行或 Kafka 不可达。
- lab 为**单节点 + 数据卷持久化**；生产目标为多 broker/副本与跨可用区部署。

## 9. 健康检查

```bash
bash services/foundation/scripts/health_check.sh
# 或：bash services/foundation/kafka/deploy/deploy_service.sh health
```

lab 健康检查为 `nc -z <host> 9092` 端口探测；compose 兜底形态另有容器级 `rpk cluster health` healthcheck。

## 10. 安全与网络边界

- **网络边界**：仅内网/隧道可达——端口只经 QEMU host-forward 暴露在本机回环与业务来宾 `10.0.2.2`，**不对公网开放**。
- **认证**：lab `PLAINTEXT` 无认证（可信内网）；生产启用 SASL/TLS，凭据来源见 §6。
- **优雅退出**：`restart unless-stopped`；`down` 走正常停止，broker 落盘后删容器，避免强杀导致日志段损坏。

## 11. SLO（lab）

| 指标 | 目标 |
|------|------|
| 可用性 | 联调时段单节点 best-effort；outbox + at-least-once 保证最终投递 |
| 投递延迟 | 端到端 P99 < 数秒（含中继轮询周期） |
| 数据安全 | 不丢已提交事件（outbox 兜底 + 数据卷持久化）；重复由消费方幂等吸收 |

> 生产级可用性 SLA 与多 broker 目标在 HA 引入后单独定义，
