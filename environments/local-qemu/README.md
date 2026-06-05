# local-qemu（Mac 本地 QEMU 联调环境）

节点 IP、SSH 转发端口、foundation 连接与 frp 配置：`nodes.env`（模板 `nodes.example.env`）。  
QEMU 运行时产物在 `vms/`（已 gitignore）。

业务服务部署入口在各自目录：`services/<service>/deploy/deploy_service.sh` 与 `services/platform/backoffice-web/deploy/deploy_service.sh`。

## 脚本清单

- 无统一服务编排脚本。
- 服务部署、镜像构建、镜像分发、容器启动均在服务目录内执行。
- 节点生命周期入口也下沉到服务目录：
  - `services/<service>/deploy/start_nodes.sh`
  - `services/<service>/deploy/check_nodes.sh`
  - `services/<service>/deploy/stop_nodes.sh`
  - `services/platform/backoffice-web/deploy/start_nodes.sh`
  - `services/platform/backoffice-web/deploy/check_nodes.sh`
  - `services/platform/backoffice-web/deploy/stop_nodes.sh`

## 强约束

- 不提供批量全服务编排脚本。
- 不维护服务清单中心脚本。
- 每个服务在自身 `deploy/deploy_service.sh` 中定义：
  - 构建目标（Bazel target 或 Docker build context）
  - 镜像名
  - 节点与端口
  - 运行参数

## 使用流程

```bash
cp environments/local-qemu/nodes.example.env environments/local-qemu/nodes.env
bash services/foundation/postgres/deploy/start_nodes.sh
bash services/foundation/postgres/deploy/check_nodes.sh
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/foundation/redis/deploy/deploy_service.sh up
bash services/foundation/kafka/deploy/deploy_service.sh up
bash services/gateway/deploy/start_nodes.sh
bash services/gateway/deploy/check_nodes.sh
bash services/gateway/deploy/deploy_service.sh
```

## Foundation 节点

本地商业化联调用单独的 `foundation` QEMU 节点承载 PostgreSQL、Redis、Kafka / Redpanda，避免基础组件与 Bazel build 节点抢资源。默认本机转发端口：

| 组件 | 来宾端口 | Mac host-forward |
|------|----------|------------------|
| PostgreSQL | `5432` | `15432` |
| Redis | `6379` | `16379` |
| Kafka / Redpanda | `9092` | `19092` |

业务 QEMU 来宾通过 `10.0.2.2:<host-forward-port>` 访问 foundation 组件；配置项见 `environments/local-qemu/nodes.env`。

## 业务节点拓扑（v1 lab）

| 环境变量 | 典型职责 | 端口 |
|----------|----------|------|
| `NODE_IP_BUILD` | Bazel 构建 + 镜像 save | SSH `2209` |
| `NODE_IP_GATEWAY` | gateway BFF | `8080` |
| `NODE_IP_USER` | user-server | `9101` |
| `NODE_IP_BACKOFFICE` | backoffice-backend（`9110`）+ recommendation-server（`9103`）同机；共享 `EXPORT_DIR` | SSH `2203` |
| `NODE_IP_TRACKING` | tracking-server；读取同路径 snapshot | `9105` |

`NODE_IP_BACKOFFICE` 与 `NODE_IP_TRACKING` 在商业化 v1 建议同节点挂载 `/var/lib/simple-living/exports`；lab 可分机，由 `SNAPSHOT_DIR` 卷同步或 NFS 代替。

## Bazel 版本

- `.bazelversion` 是唯一版本源。
- 优先 `bazelisk`，回退 `bazel`。
