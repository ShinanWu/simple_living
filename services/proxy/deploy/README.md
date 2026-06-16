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

`proxy` 采用镜像标签回滚。推荐用显式入口 `rollback.sh`（部署历史标签后自动跑 `verify_proxy.sh` 校验）：

```bash
bash services/proxy/deploy/rollback.sh v2026.04.30
# 或
IMAGE_TAG=v2026.04.30 bash services/proxy/deploy/rollback.sh
```

等价的手动方式（不含自动校验）：

```bash
IMAGE_TAG=v2026.04.30 bash services/proxy/deploy/deploy_service.sh
```

> 回滚须指定**已构建过的历史标签**；回滚仅切换入口容器镜像，不影响 gateway 与业务域。FRP/TLS 等敏感值仍通过环境变量注入，不从仓库读取。

停止当前服务相关节点：

```bash
bash services/proxy/deploy/stop_nodes.sh
```

## 2.1 TLS 证书（生产 HTTPS）

证书与私钥挂载在 nginx 来宾 `/var/lib/simple-living/tls`（容器内 `/etc/nginx/certs`），**禁止入库**。

**首次签发 / 续期**（Let's Encrypt，HTTP-01 经 frp 80 回源）：

```bash
bash services/proxy/deploy/issue_tls_cert.sh
```

脚本在 nginx 来宾安装 `certbot`、写入 webroot 挑战目录、复制证书并重启 `simple-living-proxy`。域名须已在 `nodes.env` 配置 `FRP_CUSTOM_DOMAIN=shaotang.top` 且 DNS 指向 frps 公网 IP。

**手动续期后**（若未用上述脚本）：

```bash
sudo cp /etc/letsencrypt/live/<域名>/fullchain.pem /var/lib/simple-living/tls/fullchain.pem
sudo cp /etc/letsencrypt/live/<域名>/privkey.pem   /var/lib/simple-living/tls/privkey.pem
sudo chmod 600 /var/lib/simple-living/tls/privkey.pem
sudo podman restart simple-living-proxy   # 或 docker
```

**验收**：`curl -fsS https://shaotang.top/healthz`；`openssl s_client -connect shaotang.top:443 -servername shaotang.top` 应显示 Let's Encrypt 签发者。

## 2.2 故障排查与日志

日志位置（详见 `../README.md` 可观测性章节）：Nginx `/var/log/nginx/{access,error}.log`，frpc/frps `容器内 /var/log/frp/*.log`，容器整体 `docker/podman logs simple-living-proxy`。

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| `502 Bad Gateway` | gateway upstream 不可达/未就绪 | 查 `error.log` upstream 错误；确认 `gateway:8080` Service DNS/端口与 Pod 健康 |
| `504 Gateway Timeout` | gateway 处理超时或网络抖动 | 对照 `access.log` 的 `urt`；确认本地↔K8s 连通性与 gateway SLO |
| 证书过期/握手失败 | TLS 证书到期或路径错误 | `curl -vk https://<域名>/healthz` 看有效期；核对 `ssl_certificate*` 路径与 `nginx -t` |
| 公网不通但本机通 | frpc 未连上 frps / token 不一致 | 查 frpc 日志鉴权；核对 `FRP_AUTH_TOKEN` 与 frps `auth.token`、`7000`/`FRP_REMOTE_PORT` 放行 |
| 回源协议/来源 IP 异常 | 转发头缺失 | 确认 `X-Forwarded-For`/`X-Forwarded-Proto`/`Host` 透传（见示例配置） |

详细的 frp/nginx 故障树与公网链路 `frps → frpc → nginx → gateway` 验收说明见 `../README.md` §4–§5。

## 3. 验收

QEMU 宿主机到 nginx 来宾的端口转发（见 `start_nodes.sh`）：

```bash
curl -fsS http://127.0.0.1:80/healthz
```

一键检查「来宾本机 + 经远端 frps 公网」与 `frpc` 日志（需已配置 `environments/local-qemu/nodes.env`）：

```bash
bash services/proxy/deploy/verify_proxy.sh
```

## 4. 跨服务协作原则

- 仅通过契约文档协作：`services/gateway/api.md` 与 `各服务 `api.md``。
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

### 5.4 Mac 桌面 VNC（宿主机独立 frpc）

与 §5.1–5.3 的 gateway 容器 frpc **无关**：在 **Mac 本机** 运行第二条 frpc，TCP 映射屏幕共享 `5900` → frps 公网 `15900`（端口可改）。

1. Mac 开启屏幕共享并设强密码。
2. VPS 防火墙放行 `15900`（或 `FRP_DESKTOP_VNC_REMOTE_PORT`）。
3. 启动：

```bash
FRP_AUTH_TOKEN=your_token \
FRP_SERVER_ADDR=8.152.103.12 \
bash services/proxy/deploy/start_desktop_frpc.sh
```

4. 验收与远程连接见 `../README.md` §8；`open vnc://<frps_ip>:15900`。

## 6. 配置来源

- `services/proxy/src/frp/frps.toml.example`
- `services/proxy/src/frp/frpc.toml.example`
- `services/proxy/src/frp/frpc-desktop.toml.example`
- `services/proxy/src/nginx/gateway.conf.example`（HTTP 联调入口）
- `services/proxy/src/nginx/gateway.https.conf.example`（HTTPS + HSTS 生产入口）

完整配置项表（Nginx upstream/TLS/HSTS/限流、FRP 环境变量与密钥注入方式）、SLO、安全基线见 `../README.md`。回滚脚本：`services/proxy/deploy/rollback.sh`；公网链路验收：`services/proxy/deploy/verify_proxy.sh`。
