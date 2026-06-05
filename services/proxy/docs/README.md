# 公网入口（frp + nginx + gateway）

本文档给出本地 QEMU 商业化联调与公网访问的正式入口方案，满足以下边界：

- `gateway` 是唯一对外业务入口（BFF/public API entry）。
- `HTTP(80)` 用于当前公网联调；正式生产域名必须补齐 `HTTPS(443)`、证书自动续期与 HSTS 策略。
- 仅远端入口机暴露公网入口端口（当前以 `80` 为主）。
- 本地环境不直接暴露业务端口到公网，通过 `frpc` 建立到远端 `frps` 的隧道。
- 真实密钥、证书、token 不入库；仓库仅保存 `.example` 模板。

相关模板文件：

- `services/proxy/src/frp/frps.toml.example`
- `services/proxy/src/frp/frpc.toml.example`
- `services/proxy/src/nginx/gateway.conf.example`（HTTP，当前联调入口）
- `services/proxy/src/nginx/gateway.https.conf.example`（HTTPS + HSTS，正式生产入口）

> 文档分工：本 README 是 proxy 的人工维护契约入口与运维 runbook 主文档；独立部署/回滚/验收命令见 `../deploy/README.md`。

---

## 职责与边界

`proxy` 是入口基础服务（前置 Nginx + FRP 隧道），**只承担入口层职责**，不持有任何业务真相。

### 本服务负责

- **TLS 终止**：在入口终止 HTTPS，向 `gateway` 回源（私网内 HTTP）。
- **反向代理**：按路径前缀把请求转发到 `gateway`（业务 API）与 `backoffice-web`（后台前端 SPA），见 §6。
- **基础限流**：入口层粗粒度 `limit_req` 抵御突发与扫描；细粒度/按身份的业务限流由 `gateway` 负责。
- **入口健康检查**：暴露 `/healthz`，回源到 gateway 健康接口（`../../gateway/docs/api.md` §9.19 `/api/v2/health`，本仓 `/healthz → /api/v2/health/check`）。
- **公网隧道（FRP）**：在无公网 IP/域名时，`frpc` 建立到入口机 `frps` 的隧道，公网链路为 `frps → frpc → nginx → gateway`。

### 本服务不负责（指向正确归属）

- 业务字段语义、页面聚合、JSON↔proto 映射、鉴权与会话 → `gateway`（`../../gateway/docs/`）。
- 业务路由真相（路径/版本/动作语义）→ `gateway`（`../../gateway/docs/api.md`，详见 §6.2/§6.3）。
- 对外错误信封与公共 JSON 语义 → `.cursor/rules/shared-contracts.mdc`。

> **无业务契约文档**：proxy 是入口基础服务，不拥有业务 `api.md` / `data-model.md` / `proto`，这三类文档对其**不适用**，按 DoD 仅维护 `docs/README.md` + `deploy/README.md`，运维要求同样适用。

---

## 服务等级目标（SLO）

仅承诺入口转发层目标；业务接口可用性/延迟以 gateway 为权威（`../../gateway/docs/README.md`）。

| 指标 | 目标 | 测量点 |
|------|------|--------|
| 入口可用性 | 月度 ≥ 99.9% | `/healthz` 返回 2xx 的成功率；上游 gateway 自身不可用导致的 5xx 单独归因到 gateway |
| 转发延迟（proxy 自身开销） | P50 ≤ 5ms，P99 ≤ 20ms | `$request_time - $upstream_response_time`，即 Nginx 转发额外开销，不含 gateway 处理时间 |
| 端到端延迟（含 gateway） | ≤ gateway SLO + 20ms（P99） | 以 gateway README 的 SLO 为基线叠加入口开销 |
| 入口自产 5xx 比例 | < 0.1% | 仅统计 Nginx 自身产生的 502/504（上游不可用、连接/读超时） |

---

## 配置项

真实 token / TLS 私钥 / 证书**不入库**；仓库仅保存 `.example` 模板，部署时复制到受控路径并替换真实值，注入方式见各表“来源/注入”。

### Nginx（入口反向代理）

| 配置项 | 默认/示例值 | 必填 | 来源/注入 | 说明 |
|--------|-------------|------|-----------|------|
| `upstream simple_living_gateway` | `127.0.0.1:8080`（K8s 内为 `gateway.<ns>.svc.cluster.local:8080`） | 是 | 配置文件 | 业务 API 回源，gateway 内部再路由分发 |
| `upstream simple_living_backoffice` | `127.0.0.1:8088` | 是 | 配置文件 | 后台前端 SPA 回源 |
| `keepalive`（upstream） | gateway `32` / backoffice `16` | 否 | 配置文件 | 回源长连接池 |
| `listen` | HTTP `80` / HTTPS `443 ssl http2` | 是 | 配置文件 | 入口监听端口 |
| `ssl_certificate` | `/etc/nginx/certs/fullchain.pem` | 生产必填 | 主机受控目录（权限 `600`） | 证书链 |
| `ssl_certificate_key` | `/etc/nginx/certs/privkey.pem` | 生产必填 | 主机受控目录（权限 `600`） | 私钥，禁止入库 |
| `ssl_protocols` | `TLSv1.2 TLSv1.3` | 是（HTTPS） | 配置文件 | 见“安全基线” |
| `ssl_ciphers` | 见 `gateway.https.conf.example` | 是（HTTPS） | 配置文件 | 仅保留前向安全套件 |
| `Strict-Transport-Security` (HSTS) | `max-age=31536000; includeSubDomains` | 是（HTTPS） | 配置文件 | 仅 HTTPS server 下发 |
| `limit_req_zone` / `limit_req` | `zone=ingress_req rate=20r/s`，`burst=40 nodelay` | 是 | 配置文件 | 入口粗粒度限流；`/healthz` 不限流 |
| `X-Forwarded-For` / `X-Forwarded-Proto` / `Host` | 透传 | 是 | 配置文件 | 供 gateway 还原来源/协议 |

### FRP（公网隧道，环境变量注入；见 `../deploy/deploy_service.sh`）

| 环境变量 | 默认值 | 必填 | 来源/注入 | 说明 |
|----------|--------|------|-----------|------|
| `ENABLE_FRP` | `1` | 否 | 部署命令/`nodes.env` | `0` 表示仅启 Nginx（纯商业化入口机模式） |
| `FRP_EMBEDDED` | `0` | 否 | 部署命令 | `1` = 容器内自带 frps+frpc（实验室无独立 VPS） |
| `FRP_PROXY_TYPE` | `http` | 否 | 部署命令 | `http`（需域名）或 `tcp`（公网 IP+端口） |
| `FRP_SERVER_ADDR` | 空 | 远端模式必填 | 部署命令/`nodes.env` | 远端 frps 公网地址 |
| `FRP_SERVER_PORT` | `7000` | 否 | 部署命令 | frps 控制端口 |
| `FRP_AUTH_TOKEN` | 空 | 远端模式必填 | **仅环境变量传入，禁止写入 `nodes.env`/仓库** | frpc↔frps 鉴权，须与 frps `auth.token` 一致 |
| `FRP_CUSTOM_DOMAIN` | 空 | http 模式必填 | 部署命令 | 公网访问域名（DNS 指向 frps IP） |
| `FRP_REMOTE_PORT` | `10080` | tcp 模式相关 | 部署命令 | tcp 模式公网暴露端口 |
| `FRP_USER` | `phase1-local-gateway` | 否 | 部署命令 | frpc 稳定身份标识 |
| `IMAGE_TAG` | `latest` | 否 | 部署命令 | 镜像标签，回滚时指定历史标签（见 `../deploy/README.md`） |

> `FRP_AUTH_TOKEN` 等敏感值通过部署命令前缀的环境变量注入（`deploy_service.sh` 会在 source `nodes.env` 后保留调用方导出的 token），保证密钥不落入版本库与 env 文件。

---

## 可观测性与指标

### 健康检查

入口健康检查命令见 §4；无人值守可用 `bash services/proxy/src/scripts/check_ingress.sh https://<域名或IP>` 或 `bash services/proxy/deploy/verify_proxy.sh`。

### 日志位置

| 日志 | 路径 | 说明 |
|------|------|------|
| Nginx 访问日志 | `/var/log/nginx/access.log`（`ingress` 格式，含 `rt`/`urt`/`status`/`xff`） | 计算可用性、转发/端到端延迟、5xx 比例 |
| Nginx 错误日志 | `/var/log/nginx/error.log` | 上游不可达、TLS 握手、超时排查 |
| frpc 日志 | 容器内 `/var/log/frp/frpc.log` | 隧道连接/鉴权状态 |
| frps 日志（内嵌模式） | 容器内 `/var/log/frp/frps.log` | 内嵌 frps 控制面 |

容器化部署可用 `docker/podman logs simple-living-proxy` 查看入口与 frp 启动输出。

### 关键指标

| 指标 | 来源/计算 | 用途 |
|------|-----------|------|
| 入口可用性 | `/healthz` 2xx 占比 | 对照 SLO 99.9% |
| 转发延迟 | 访问日志 `rt - urt`（`$request_time - $upstream_response_time`） | proxy 自身开销 |
| 端到端延迟 | 访问日志 `rt`（`$request_time`） | 含 gateway 的整体时延 |
| 入口 5xx | 访问日志 `status` 中 502/504 计数占比 | 上游不可用/超时告警 |
| frpc 连接状态 | frpc 日志 `login to server success`/重连 | 公网链路连通性 |

---

## 安全基线

- **TLS 版本**：仅 `TLSv1.2`/`TLSv1.3`，禁用更低版本（见 `gateway.https.conf.example`）。
- **加密套件**：仅保留前向安全（ECDHE）套件，`ssl_prefer_server_ciphers on`。
- **HSTS**：HTTPS server 下发 `Strict-Transport-Security max-age=31536000; includeSubDomains`；HTTP 仅做 `301` 跳转到 HTTPS。
- **入口限流**：`limit_req` 粗粒度防突发/扫描；细粒度业务限流在 gateway。
- **最小暴露端口**：仅入口机暴露 `80/443`（及 FRP 必需的 `7000` 控制端口、tcp 模式 `FRP_REMOTE_PORT`），并按需限制来源；本地业务服务不直接对公网暴露。
- **密钥与证书**：真实 token、TLS 私钥/证书禁止入库，仅放主机受控目录并设最小权限（`600`）。
- **统一入口**：对外业务请求一律经 `gateway`，禁止绕过网关直连内部域服务。

---

## 1. 远端 frps 部署（公网入口机）

远端主机示例：`8.152.103.12`

1) 安装并准备目录（示例）

```bash
sudo mkdir -p /etc/frp
sudo cp services/proxy/src/frp/frps.toml.example /etc/frp/frps.toml
sudo chmod 600 /etc/frp/frps.toml
```

2) 编辑 `/etc/frp/frps.toml`

- 替换 `auth.token` 为强随机值。
- 监听端口保持：
  - `bindPort = 7000`
  - `vhostHTTPPort = 80`
  - `vhostHTTPSPort = 443`

3) systemd 服务（示例）

`/etc/systemd/system/frps.service`：

```ini
[Unit]
Description=frp server
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/frps -c /etc/frp/frps.toml
Restart=always
RestartSec=3
LimitNOFILE=1048576

[Install]
WantedBy=multi-user.target
```

4) 启动与检查

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now frps
sudo systemctl status frps --no-pager
sudo ss -lntp | rg ':(7000|80|443)\b'
```

> 安全要求：公网仅需放行 `7000/80/443`（按需限制来源）；不要在仓库存放真实 token。

---

## 2. 本地 frpc（systemd）管理

本地机器负责连接远端 `frps`，并把远端 `http` 映射到本地 `nginx 80`。

1) 准备配置

```bash
sudo mkdir -p /etc/frp
sudo cp services/proxy/src/frp/frpc.toml.example /etc/frp/frpc.toml
sudo chmod 600 /etc/frp/frpc.toml
```

2) 编辑 `/etc/frp/frpc.toml`

- `serverAddr = "8.152.103.12"`
- `serverPort = 7000`
- `auth.token` 必须与远端 `frps` 一致

3) systemd 服务（示例）

`/etc/systemd/system/frpc.service`：

```ini
[Unit]
Description=frp client
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/frpc -c /etc/frp/frpc.toml
Restart=always
RestartSec=3
LimitNOFILE=1048576

[Install]
WantedBy=multi-user.target
```

4) 启停与日志

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now frpc
sudo systemctl status frpc --no-pager
sudo journalctl -u frpc -n 100 --no-pager
```

---

## 3. nginx 启停与验证（本地私网侧）

`nginx` 在本地终止 TLS，然后反代到 K8s `gateway` Service。

1) 安装配置

```bash
sudo cp services/proxy/src/nginx/gateway.conf.example /etc/nginx/conf.d/gateway.conf
sudo nginx -t
sudo systemctl enable --now nginx
```

2) 准备证书（示例路径）

- 证书：`/etc/nginx/tls/fullchain.pem`
- 私钥：`/etc/nginx/tls/privkey.pem`

> 真实证书与私钥禁止入库，只允许放在主机受控目录并设置最小权限（如 `600`）。

3) 常用启停命令

```bash
sudo systemctl restart nginx
sudo systemctl status nginx --no-pager
sudo journalctl -u nginx -n 100 --no-pager
```

---

## 4. 健康检查命令

### 4.1 本地侧

```bash
sudo ss -lntp | rg ':(80|443)\b'
curl -vk https://127.0.0.1/healthz
curl -v http://127.0.0.1/healthz
```

### 4.2 远端入口机侧

```bash
sudo ss -lntp | rg ':(7000|80)\b'
curl -v http://8.152.103.12/healthz
```

### 4.3 链路检查建议

- `frps` 正常监听 `7000/80`
- `frpc` 状态为 `active (running)`，日志无鉴权失败
- `nginx -t` 通过，且能把请求转发到 `gateway` Service
- `gateway` 返回业务健康状态（例如 `/healthz`）

可直接使用脚本（适合无人值守）：

```bash
bash services/proxy/src/scripts/check_ingress.sh https://<your_domain_or_ip>
```

---

## 5. 常见故障排查

1) `frpc` 连接失败

- 现象：`connection refused` / `i/o timeout`
- 检查：
  - 远端 `frps` 是否运行：`systemctl status frps`
  - 网络/防火墙是否放行 `7000`
  - `serverAddr/serverPort` 是否正确

2) 鉴权失败

- 现象：`token` related auth error
- 检查：
  - `frps` 与 `frpc` 的 `auth.token` 是否完全一致
  - 配置文件是否加载了最新内容（重启服务后再看日志）

3) HTTPS 暂未启用（当前阶段）

- 现象：访问 `https://` 失败或握手异常
- 说明：当前交付默认仅保证 `http://` 链路；HTTPS 待域名/SNI/证书就绪后开启

4) 访问 443 通但业务不通

- 现象：`502/504`
- 检查：
  - `upstream` 指向的 `gateway` Service DNS/端口是否正确
  - 本地到 K8s 网络连通性
  - `gateway` Pod/Service 是否健康

5) 请求头丢失导致网关识别异常

- 现象：回源后协议、来源 IP 判断异常
- 检查：
  - `X-Forwarded-For`、`X-Forwarded-Proto`、`Host` 是否在 nginx 中透传

---

## 6. 路由约定（Nginx ↔ Gateway）

### 6.1 路由转发策略

Nginx 做**路径前缀匹配**，将请求分发到对应的 upstream：

| 路径前缀 | Upstream | 目标端口 | 说明 |
|----------|----------|----------|------|
| `/api/` | `simple_living_gateway` | 8080 | 业务 API，由 gateway 内部路由分发 |
| `/healthz` | `simple_living_gateway` | 8080 | 健康检查 |
| `/backoffice` | `simple_living_backoffice` | 8088 | 后台管理前端 SPA |

**示例配置**（见 `src/nginx/gateway.conf.example`）：

```nginx
upstream simple_living_gateway {
    server 127.0.0.1:8080;
    keepalive 32;
}

upstream simple_living_backoffice {
    server 127.0.0.1:8088;
    keepalive 16;
}

location /api/ {
    proxy_pass http://simple_living_gateway;
    proxy_http_version 1.1;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto https;
    proxy_set_header Connection "";
}

location /backoffice {
    proxy_pass http://simple_living_backoffice;
    proxy_http_version 1.1;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto https;
    proxy_set_header Connection "";
}
```

> **注意**：`/backoffice` 的 `proxy_pass` 不带尾部斜杠，路径原样传递给 backoffice-web 容器，因为前端构建时 `base: "/backoffice/"`，资源路径以 `/backoffice/` 开头。
>
> 上面为路由片段示意；完整可上线配置（`ingress` 访问日志格式、`limit_req` 入口限流、`/healthz` 关闭限流、HTTPS 的 `ssl_ciphers`/HSTS）以 `src/nginx/gateway.conf.example` 与 `gateway.https.conf.example` 为准。

### 6.2 路由真相在 Gateway

| 层级 | 职责 | 变更频率 |
|------|------|----------|
| **Nginx** | TLS 终止、限流、反向代理前缀、静态资源缓存 | 极低（仅新增模块级前缀时） |
| **Gateway** | 业务路由注册、鉴权、协议转换、BFF 聚合 | 日常迭代 |

- 新增 backoffice 路由（如 `/api/v2/backoffice/content/items/update`）→ **只改 gateway 文档和代码**
- 新增模块级前缀（如未来拆分 `/api/v2/admin/` 独立 upstream）→ 才需要改 Nginx
- Nginx 配置保持简洁稳定，不承载业务路由语义

### 6.3 路由维护约定

- 所有业务路由的增删改只在 gateway 的 `services/gateway/docs/api.md` 和代码中维护
- Nginx 不定义任何业务路径，仅定义 `location` 前缀匹配规则
- 如需新增独立 upstream（如独立部署 backoffice 服务），需同步更新：
  1. Nginx upstream 配置
  2. Nginx location 匹配规则
  3. Gateway 文档（说明路由迁移）

---

## 7. 安全与边界要求（必须遵守）

- 真实 `token`、TLS 私钥、证书文件禁止提交到仓库。
- `.example` 文件仅用于模板演示，部署时必须复制到受控路径并替换真实值。
- `80/443` 仅在远端入口机暴露；本地业务服务不直接对公网暴露。
- 对外业务请求统一经 `gateway` 进入，避免绕过网关直接访问内部域服务。

---

## 8. 变更记录

proxy 无独立 `changelog.md`，契约/行为变更在此记录（破坏性变更需显式标注并列出受影响方）。

| 日期 | 变更 | 影响方 |
|------|------|--------|
| 2026-06-04 | 补齐上线级文档：新增「职责与边界、SLO、配置项表、可观测性与指标、安全基线」章节；Nginx 示例新增 `ingress` 访问日志格式、`limit_req` 入口限流，HTTPS 示例补 `ssl_ciphers`；新增 `deploy/rollback.sh` 与证书续期/故障排查 runbook。 | 运维（部署/回滚/排查流程更新）；无对外 JSON 契约变更 |
