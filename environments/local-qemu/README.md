# local-qemu（Mac 本地 QEMU 联调环境）

节点 SSH、服务端口与 foundation 连接：`nodes.env`（模板 `nodes.example.env`）。  
**服务端口唯一来源**：`lab-ports.env`（来宾、容器 host 网络、K8s Service 三者同号）。

QEMU 磁盘与日志在 `vms/`（已 gitignore）。启动来宾：`services/<service>/deploy/start_nodes.sh`。

## 服务端口与命名


| 服务目录                                   | QEMU 来宾名                | 容器名                                   | brpc/HTTP 端口           | Mac 转发 (localhost→来宾)           | SSH   |
| -------------------------------------- | ----------------------- | ------------------------------------- | ---------------------- | ------------------------------- | ------ |
| `services/gateway`                     | `gateway`               | `simple-living-gateway`               | `8080`                 | `8080→8080`（公网入口在 `nginx` 来宾）   | `2201` |
| `services/user-server`                 | `user-server`           | `simple-living-user-server`           | `9101`                 | `9101→9101`                     | `2202` |
| `services/platform/backoffice-backend` | `backoffice-backend`    | `simple-living-backoffice-backend`    | `9110`                 | `9110→9110`                     | `2203` |
| `services/recommendation-server`       | `recommendation-server` | `simple-living-recommendation-server` | `9103`                 | `9103→9103`                     | `2204` |
| `services/tracking-server`             | `tracking-server`       | `simple-living-tracking-server`       | `9105`                 | `9105→9105`                     | `2206` |
| `services/proxy` + 公网入口                | `nginx`                 | `simple-living-proxy` 等               | `80` / `8080` / `8088` | 见 `proxy/deploy/start_nodes.sh` | `2208` |
| 构建                                     | `build`                 | —                                     | —                      | —                               | `2209` |
| foundation                             | `foundation`            | —                                     | `5432`/`6379`/`9092`   | `15432`/`16379`/`19092`         | `2211` |


brpc 服务在来宾上使用 `**--network host`**：容器监听端口与来宾端口相同，无需 `-p 9103:9103` 二次映射。

`nginx` 来宾上的 gateway 通过 `10.0.2.2:<服务端口>` 访问其它来宾（`lab-ports.env` 中 `LAB_QEMU_GATEWAY_HOST`）；地址由 `tools/lab_gateway_addrs.sh` 生成并注入 gateway 容器环境变量。

## 部署顺序（商业化联调）

```bash
cp environments/local-qemu/nodes.example.env environments/local-qemu/nodes.env
# 按需启动来宾后：
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/platform/backoffice-backend/deploy/deploy_service.sh
bash services/recommendation-server/deploy/deploy_service.sh   # 需与 backoffice 同步 SNAPSHOT_DIR
bash services/user-server/deploy/deploy_service.sh
bash services/tracking-server/deploy/deploy_service.sh
bash services/gateway/deploy/deploy_service.sh
```

验收：

```bash
BASE_URL=http://8.152.103.12 python3 client/tests/gateway_api_smoke.py
```

## Bazel

- `.bazelversion` 为唯一版本源；优先 `bazelisk`。

