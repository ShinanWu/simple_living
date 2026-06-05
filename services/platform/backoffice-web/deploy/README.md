# backoffice-web 部署说明

## 1. QEMU 节点脚本

```bash
bash services/platform/backoffice-web/deploy/start_nodes.sh
bash services/platform/backoffice-web/deploy/check_nodes.sh
bash services/platform/backoffice-web/deploy/deploy_service.sh
```

仅处理 `backoffice-web`，不会构建或发布其他服务。

回滚：

```bash
IMAGE_TAG=v2026.04.27 bash services/platform/backoffice-web/deploy/deploy_service.sh
```

停止：

```bash
bash services/platform/backoffice-web/deploy/stop_nodes.sh
```

验收：

```bash
curl -fsS "http://127.0.0.1:18081" | sed -n '1,10p'
```

## 2. 本地 Docker 构建

在仓库根目录：

```bash
docker build -t simple-living-backoffice-web:latest services/platform/backoffice-web
docker run --rm -p 8088:8088 simple-living-backoffice-web:latest
```

访问 `http://127.0.0.1:8088`。

## 3. 与 Gateway 联调

运行时默认 `HttpGatewayApiClient`；`VITE_GATEWAY_BASE_URL` 构建期注入。生产推荐留空（同源），由前置 nginx 反代 `/api/v2/backoffice/*` → gateway。`FakeGatewayRepository` 仅组件测试夹具。

契约权威：`../../gateway/docs/backoffice-backend.md` §3、`../../gateway/docs/api.md` §13.6。

## 4. QEMU Lab 镜像分发（可选）

1. 在 build 节点构建：

```bash
ssh -p 2209 ubuntu@127.0.0.1 \
  "cd /home/ubuntu/simple_living && podman build --network=host --pull=false -f services/platform/backoffice-web/Dockerfile -t simple-living-backoffice-web:latest services/platform/backoffice-web"
```

2. 分发到 nginx 节点并运行（见 `environments/local-qemu/README.md`）。

## 5. 协作原则

- 仅通过契约协作：`.cursor/rules/shared-contracts.mdc` 与 `services/gateway/docs/backoffice-backend.md`。
- 不依赖 C 端 `client/` 文档或实现；与 `backoffice-web` 无代码共享关系。
