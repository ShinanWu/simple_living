# gateway 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/gateway/deploy/start_nodes.sh
bash services/gateway/deploy/check_nodes.sh
bash services/gateway/deploy/deploy_service.sh
```

仅处理 `gateway`，不会构建或发布其他服务。

## 2. 回滚

回滚到指定历史镜像 tag（重新部署旧版本）：

```bash
IMAGE_TAG=v2026.04.27 bash services/gateway/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/gateway/deploy/stop_nodes.sh
```

回滚要点：

- gateway **无状态**，无库表迁移，回滚只需切回旧镜像 tag，无需数据回滚。
- 回滚后用 §3 验收命令确认 `/healthz` 与 `/api/v2/health` 正常、对外信封符合 [api.md](../api.md)。
- K8s 环境用 `services/gateway/deploy/k8s/rollback.sh`（见 §6）。
- 先确认下游各业务域版本与对外契约兼容，避免回滚网关后下游不匹配。

## 3. 验收

健康检查（容器内/反代后均应通过）：

```bash
# 内部存活探针（明文 HTTP）
ssh -p 2208 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:8080/healthz"

# 对外公开健康面（标准信封，经前置 Nginx 时走 HTTPS）
curl -fsS http://127.0.0.1:8080/api/v2/health/check -H 'Content-Type: application/json' -d '{}'
```

`/api/v2/health` 期望返回 `success:true`、`code:0`，`data.components[]` 反映网关与可探活下游状态（见 [api.md](../api.md) §9.19）。

冒烟一条典型成功路径与一条错误码，确认对外信封不变量（`code===0 ⇔ success===true`）：

```bash
curl -fsS http://127.0.0.1:8080/api/v2/guest/session \
  -H 'Content-Type: application/json' \
  -d '{"device_id":"device_smoke","client_platform":"ios"}'

curl -s -o /dev/null -w '%{http_code}\n' http://127.0.0.1:8080/api/v2/__nope__
```

## 4. 镜像构建与运行说明

`services/gateway/deploy/deploy_service.sh` 已内置以下流程：

1. 在 build 节点执行 Bazel 构建：`//services/gateway:gateway_edge_server`
2. 使用 `services/gateway/deploy/Dockerfile.cpp-service` 构建镜像：`localhost/gateway:${IMAGE_TAG}`
3. 将镜像从 build 节点分发到 `nginx` 来宾（`NODE_IP_NGINX`）
4. 运行容器：`simple-living-gateway`（`8080:8080`）

下游 brpc 地址由 `tools/lab_gateway_addrs.sh` 根据 `environments/local-qemu/lab-ports.env` 注入，例如：

- `GATEWAY_USER_SERVER_ADDR=10.0.2.2:9101`
- `GATEWAY_RECOMMENDATION_SERVER_ADDR=10.0.2.2:9103`
- `GATEWAY_TRACKING_SERVER_ADDR=10.0.2.2:9105`
- `GATEWAY_BACKOFFICE_BACKEND_ADDR=10.0.2.2:9110`

## 5. 跨服务协作原则

- 终端 JSON：`services/gateway/api.md`；域 RPC：各服务根目录 `api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 6. K8s（服务内部署入口）

```bash
bash services/gateway/deploy/k8s/apply.sh
bash services/gateway/deploy/k8s/rollback.sh
```

## 7. 日志

gateway 输出结构化 JSON 访问/错误日志到容器 stdout/stderr；QEMU：

```bash
ssh -p 2201 ubuntu@127.0.0.1 "podman logs --tail=200 simple-living-gateway"
```

日志含 `request_id`、`trace_id`、method、path、对外 `code`、HTTP status、耗时。**不**记录令牌明文、敏感 body 与内部栈。

## 8. 排障

| 症状 | 可能原因 | 排查 |
|------|----------|------|
| `/healthz` 不通 | 容器未起 / 端口映射错 | `podman ps`、查启动日志 |
| 大量 `90002` | 下游超时 | 核对 §4 下游地址与各域健康 |
| 大量 `90003` | 下游错误 | 按 `request_id` 查日志，对照下游 `api.md` |
| 持续 `10005` | 限流 | 调 `-ratelimit_*` flag |
| 整页失败 `10054` | 关键下游不可用 | 恢复 recommendation/content/tracking |

定位顺序：`/api/v2/health` → 按 `request_id` 查日志 → 对照下游 `api.md` → 回滚（§2）。
