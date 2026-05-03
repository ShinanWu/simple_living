# Phase1 最小公网入口（frp + nginx + gateway，当前 HTTP-only）

本文档给出 Phase1 可落地的最小公网入口方案样例，满足以下边界：

- `gateway` 是唯一对外业务入口（BFF/public API entry）。
- 当前阶段仅保证 `HTTP(80)` 对外可用；`HTTPS(443)` 暂不作为验收门槛。
- 仅远端入口机暴露公网入口端口（当前以 `80` 为主）。
- 本地环境不直接暴露业务端口到公网，通过 `frpc` 建立到远端 `frps` 的隧道。
- 真实密钥、证书、token 不入库；仓库仅保存 `.example` 模板。

相关样例文件：

- `services/proxy/src/frp/frps.toml.example`
- `services/proxy/src/frp/frpc.toml.example`
- `services/proxy/src/nginx/gateway.conf.example`

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

## 6. 安全与边界要求（必须遵守）

- 真实 `token`、TLS 私钥、证书文件禁止提交到仓库。
- `.example` 文件仅用于模板演示，部署时必须复制到受控路径并替换真实值。
- `80/443` 仅在远端入口机暴露；本地业务服务不直接对公网暴露。
- 对外业务请求统一经 `gateway` 进入，避免绕过网关直接访问内部域服务。
