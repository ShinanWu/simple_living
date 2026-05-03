# gateway 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/gateway/deploy/start_nodes.sh
bash services/gateway/deploy/check_nodes.sh
bash services/gateway/deploy/deploy_service.sh
```

仅处理 `gateway`，不会构建或发布其他服务。

## 2. 回滚

```bash
IMAGE_TAG=v2026.04.27 bash services/gateway/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/gateway/deploy/stop_nodes.sh
```

## 3. 验收

```bash
ssh -p 2201 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:18080/healthz"
```

## 4. 镜像构建与运行说明

`services/gateway/deploy/deploy_service.sh` 已内置以下流程：

1. 在 build 节点执行 Bazel 构建：`//services/gateway:gateway_edge_server`
2. 使用 `services/gateway/deploy/Dockerfile.cpp-service` 构建镜像：`localhost/gateway:${IMAGE_TAG}`
3. 将镜像从 build 节点分发到 gateway 节点
4. 在 gateway 节点运行容器：`simple-living-gateway`

默认运行参数（容器内下游地址）：

- `GATEWAY_USER_DOMAIN_ADDR=10.0.2.2:19101`
- `GATEWAY_CONTENT_DOMAIN_ADDR=10.0.2.2:19102`
- `GATEWAY_RECOMMENDATION_DOMAIN_ADDR=10.0.2.2:19103`
- `GATEWAY_TRACKING_DOMAIN_ADDR=10.0.2.2:19105`

默认端口映射：

- Host `8080` -> Container `8080`

## 5. 跨服务协作原则

- 仅通过契约文档协作：`docs/contracts/` 与 `services/*/docs/api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 6. K8s（服务内部署入口）

```bash
bash services/gateway/deploy/k8s/apply.sh
bash services/gateway/deploy/k8s/rollback.sh
```
