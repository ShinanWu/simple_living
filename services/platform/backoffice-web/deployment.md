# backoffice-web 部署说明（v1）

## 1. 本地构建镜像

在仓库根目录执行：

```bash
docker build -t simple-living-backoffice-web:latest services/platform/backoffice-web
```

运行容器：

```bash
docker run --rm -p 8088:8088 simple-living-backoffice-web:latest
```

访问：

`http://127.0.0.1:8088`

## 2. 与 Gateway 联调

当前 backoffice-web 默认使用 `FakeGatewayRepository`。要联调真实网关，请将 `services/platform/backoffice-web/src/App.tsx` 中仓储实例切换为 `HttpGatewayApiClient`，并提供可访问的网关地址。

建议网关路由（与当前代码一致）：

- `GET /api/v2/backoffice/affiliate/partners`
- `POST /api/v2/backoffice/affiliate/partners/add`
- `GET /api/v2/backoffice/content/items`
- `PATCH /api/v2/backoffice/content/items/status`
- `GET /api/v2/backoffice/governance/reviews`
- `PATCH /api/v2/backoffice/governance/reviews/status`

## 3. QEMU Lab 部署建议

如使用 `infra/lab`，可将该容器部署在 `NODE_IP_NGINX` 节点（或单独新增 `NODE_IP_BACKOFFICE_WEB`）：

1. 在 QEMU **build** 节点构建（推荐开启 host 网络，避免容器内网络受限）：

```bash
ssh -p 2209 ubuntu@127.0.0.1 \
  "cd /home/ubuntu/simple_living && podman build --network=host --pull=false -f services/platform/backoffice-web/Dockerfile -t simple-living-backoffice-web:latest services/platform/backoffice-web"
```

2. 从 build 节点分发到 nginx 节点：

```bash
ssh -p 2209 ubuntu@127.0.0.1 "podman save localhost/simple-living-backoffice-web:latest" \
  | ssh -p 2208 ubuntu@127.0.0.1 "sudo podman load"
```

3. 在 nginx 节点运行（rootful 绑定 80 端口，便于复用 QEMU `18081->80` 转发）：

```bash
ssh -p 2208 ubuntu@127.0.0.1 \
  "sudo podman run -d --name simple-living-backoffice-web-root -p 80:8088 localhost/simple-living-backoffice-web:latest"
```

4. 访问地址（宿主机）：

`http://127.0.0.1:18081`

## 4. 链路检查

最小链路（网关和 Web 分别检查）：

```bash
curl -fsS "http://127.0.0.1:18081" | sed -n '1,10p'
```
