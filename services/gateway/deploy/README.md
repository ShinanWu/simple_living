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

- gateway **无状态**，无库表迁移，回滚只需切回旧镜像 tag，无需数据回滚（业务真相在各域，见 [`data-model.md`](../docs/data-model.md) §5）。
- 回滚后用 §3 验收命令确认 `/healthz` 与 `/api/v2/health` 正常、对外信封符合契约。
- K8s 环境用 `services/gateway/deploy/k8s/rollback.sh`（见 §6）。
- 先确认下游各业务域版本与对外契约兼容（破坏性 proto 变更须先评估），避免回滚网关后下游不匹配。

## 3. 验收

健康检查（容器内/反代后均应通过）：

```bash
# 内部存活探针（明文 HTTP）
ssh -p 2208 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:8080/healthz"

# 对外公开健康面（标准信封，经前置 Nginx 时走 HTTPS）
curl -fsS http://127.0.0.1:8080/api/v2/health/check -H 'Content-Type: application/json' -d '{}'
```

`/api/v2/health` 期望返回 `success:true`、`code:0`，`data.components[]` 反映网关与可探活下游状态（见 [`api.md`](../docs/api.md) §9.19）。

冒烟一条典型成功路径与一条错误码，确认对外信封不变量（`code===0 ⇔ success===true`）：

```bash
# 成功路径
curl -fsS http://127.0.0.1:8080/api/v2/guest/session \
  -H 'Content-Type: application/json' \
  -d '{"device_id":"device_smoke","client_platform":"ios"}'

# 路由不存在 → 期望 code 10050、HTTP 404
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

- 仅通过契约文档协作：`.cursor/rules/shared-contracts.mdc` 与 `services/*/docs/api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 6. K8s（服务内部署入口）

```bash
bash services/gateway/deploy/k8s/apply.sh
bash services/gateway/deploy/k8s/rollback.sh
```

## 7. 日志

- gateway 输出结构化 JSON 访问/错误日志到容器 stdout/stderr；QEMU 形态下查看：

```bash
ssh -p 2201 ubuntu@127.0.0.1 "podman logs --tail=200 simple-living-gateway"
# K8s：kubectl logs deploy/gateway -n simple-living --tail=200
```

- 每条日志含 `request_id`、`trace_id`、method、path、对外 `code`、HTTP status、耗时与命中下游概要，可按 `request_id` 串联整条链路（字段见 [`development.md`](../docs/development.md) §5.2）。
- **不**记录令牌明文、敏感 body 与内部栈。

## 8. 排障

| 症状 | 可能原因 | 排查 / 处理 |
|------|----------|-------------|
| `/healthz` 不通 | 容器未起 / 端口映射错 | `podman ps`、确认 Host `8080` → Container `8080` 映射；查启动日志 |
| 大量 `90002`（依赖超时） | 下游域不可达或慢 | 核对下游地址 env（§4）、下游各域健康；必要时调 `-rpc_timeout_ms` 并扩下游容量 |
| 大量 `90003`（依赖错误） | 下游返回非预期错误 | 按 `request_id`/`trace_id` 串联日志定位失败下游；查对方 `api.md` 错误语义 |
| 持续 `10005`（限流） | 真实超阈或阈值偏低 | 核对 [`README.md`](../docs/README.md) §7.5 阈值与 `-ratelimit_*` flag；区分攻击流量与正常增长 |
| `20002`/`20003` 异常增多 | 令牌时钟/撤销/user-server 异常 | 检查 user-server 健康与 `IntrospectAccessToken`；确认未误缓存过期令牌内省 |
| 首页/详情整页失败 `10054` | 关键下游（推荐/内容/tracking）不可用 | 定位失败下游并恢复；确认聚合关键依赖划分（[`workflow.md`](../docs/workflow.md) §2.6.1） |
| 下游地址连不上 | env/flag 覆盖未生效或 DNS 解析失败 | 确认 flag 优先于 env；K8s 用 Service DNS、QEMU 用 `10.0.2.2:<port>`（§4） |

定位顺序：先看 `/api/v2/health` 组件状态 → 按 `request_id` 拉取该请求全链路日志 → 对照下游域 `api.md` 错误码 → 必要时降阈/扩容/回滚（§2）。
