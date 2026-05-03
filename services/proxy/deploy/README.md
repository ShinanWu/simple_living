# proxy 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/proxy/deploy/start_nodes.sh
bash services/proxy/deploy/check_nodes.sh
bash services/proxy/deploy/deploy_service.sh
```

仅处理 `proxy`，不会构建或发布其他服务。
当前采用单镜像部署：一个容器内同时包含 `nginx + frp`，由环境变量控制 frp 是否启用。

## 2. 回滚

`proxy` 支持镜像标签回滚：

```bash
IMAGE_TAG=v2026.04.30 bash services/proxy/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/proxy/deploy/stop_nodes.sh
```

## 3. 验收

QEMU 宿主机到 nginx 来宾的端口转发（见 `start_nodes.sh`）：

```bash
curl -fsS http://127.0.0.1:18081/healthz
```

一键检查「来宾本机 + 经远端 frps 公网」与 `frpc` 日志（需已配置 `infra/lab/nodes.env`）：

```bash
bash services/proxy/deploy/verify_proxy.sh
```

## 4. 跨服务协作原则

- 仅通过契约文档协作：`docs/contracts/` 与 `services/*/docs/api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 5. frp 开关（同镜像内）

默认始终启动 nginx；frp 通过环境变量控制。

### 5.1 远端 frps（公网 VPS）— HTTP 域名

```bash
ENABLE_FRP=1 \
FRP_EMBEDDED=0 \
FRP_PROXY_TYPE=http \
FRP_SERVER_ADDR=8.152.103.12 \
FRP_SERVER_PORT=7000 \
FRP_AUTH_TOKEN=your_token \
FRP_CUSTOM_DOMAIN=ingress.example.com \
bash services/proxy/deploy/deploy_service.sh
```

浏览器访问的 Host 需与 `FRP_CUSTOM_DOMAIN` 一致（DNS 指向 frps 所在公网 IP）。

### 5.2 远端 frps — TCP（公网 IP + 端口，无需域名）

在 frps 上放行 `FRP_REMOTE_PORT`（默认 `10080`）后：

```bash
ENABLE_FRP=1 \
FRP_EMBEDDED=0 \
FRP_PROXY_TYPE=tcp \
FRP_SERVER_ADDR=your.public.ip \
FRP_SERVER_PORT=7000 \
FRP_AUTH_TOKEN=your_token \
FRP_REMOTE_PORT=10080 \
bash services/proxy/deploy/deploy_service.sh
```

外网访问：`http://your.public.ip:10080/`（或你自定义的 `FRP_REMOTE_PORT`）。

### 5.3 单容器自带 frps + frpc（实验室 / 无独立 frps 时）

容器内启动 `frps`，`frpc` 以 TCP 把本机 nginx `80` 映射到容器 `FRP_REMOTE_PORT`（默认 `10080`）。需把宿主机（或云主机）的 `10080`（及可选 `7000`）暴露到公网（安全组 / 防火墙 / 路由器端口转发）。

```bash
ENABLE_FRP=1 \
FRP_EMBEDDED=1 \
FRP_PROXY_TYPE=tcp \
FRP_REMOTE_PORT=10080 \
FRP_AUTH_TOKEN=optional_fixed_token_or_omit_to_auto_generate \
bash services/proxy/deploy/deploy_service.sh
```

QEMU 本仓库 `start_nodes.sh` 已将本机 `11080 -> 来宾 10080`、`17000 -> 来宾 7000` 转发，便于在 Mac 上验收：`curl -fsS http://127.0.0.1:11080/healthz`。

关闭 frp（商业化仅保留 nginx）：

```bash
ENABLE_FRP=0 bash services/proxy/deploy/deploy_service.sh
```

## 6. 配置来源

- `services/proxy/src/frp/frps.toml.example`
- `services/proxy/src/frp/frpc.toml.example`
- `services/proxy/src/nginx/gateway.conf.example`
