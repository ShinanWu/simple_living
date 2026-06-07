# 运营管理平台

`services/platform/` 是少糖**运营管理后台**的总目录，面向内部运营、审核与联盟配置人员，管理导购内容、治理审核与联盟伙伴，并将变更同步到 C 端读路径。

## 组成

| 目录 | 角色 | 职责 |
|------|------|------|
| [`backoffice-web/`](../backoffice-web/) | **UI** | 运营工作台（React SPA）；经 gateway 调用 API，不持久化业务数据 |
| [`backoffice-backend/`](../backoffice-backend/) | **后端** | content + governance + affiliate 权威写面；PostgreSQL + snapshot 导出 |

对外 HTTP 契约：[`gateway/docs/backoffice-backend.md`](../../gateway/docs/backoffice-backend.md)。产品与 design 见下文文档索引；不在此复制 JSON 字段表。

## 架构

```text
运营人员浏览器
  │ HTTPS/HTTP
  v
proxy（nginx [+ frp]）
  ├─ /backoffice/*  → backoffice-web :8088（静态 SPA）
  └─ /api/*         → gateway :8080（BFF）
                          │ brpc + proto
                          v
                    backoffice-backend :9110
                      content │ governance │ affiliate → PostgreSQL
                      ExportPublisher → {export_dir}/active/
                          │
          ┌───────────────┴───────────────┐
          v                               v
recommendation-server :9103      tracking-server :9105
（catalog + visibility）          （affiliate_link_spec）
          │
          v
     C 端客户端
```

| 组件 | 负责 | 不负责 |
|------|------|--------|
| backoffice-web | UI 编排、表单校验、操作日志展示 | 业务数据、鉴权规则 |
| gateway | JSON↔proto、路由、鉴权、响应信封 | 业务 OLTP、导出文件 |
| backoffice-backend | 写面权威、审核/可见性门闸、导出 | C 端读 RPC、UI |
| proxy | 反代、入口限流、frp 隧道 | 业务语义 |

**与 C 端**：不承载 C 端页面，与 `client/` 无代码耦合。C 端不直连运营库或 backoffice-backend RPC。

**运营写路径**

```text
GET  /backoffice/*          → nginx → backoffice-web
POST /api/v2/backoffice/*   → nginx → gateway → backoffice-backend → PostgreSQL
```

前端生产构建时 `VITE_GATEWAY_BASE_URL` **留空**（同源 `/api/v2/backoffice/*`）；跨域联调时构建期注入 gateway 地址。

**发布后 C 端同步**（数据面，非 RPC）

```text
backoffice-backend 写事务 → outbox → 导出 staging/ → active/
  → recommendation-server / tracking-server 热加载 manifest
```

| 导出 bundle | 消费方 |
|-------------|--------|
| `catalog_snapshot` | recommendation-server |
| `visibility_index` | recommendation-server |
| `affiliate_link_spec` | tracking-server（不含签名密钥） |

**硬约束**：`-export_dir`（backoffice-backend）与 `-snapshot_dir`（recommendation-server）必须同节点共享挂载（默认 `/var/lib/simple-living/exports/`）。

## 部署

### 公网入口（联调示例）

| URL | 目标 |
|-----|------|
| `http://<入口>/backoffice/` | backoffice-web :8088 |
| `http://<入口>/api/v2/backoffice/*` | gateway :8080 |

路由约定见 [`proxy/docs/README.md`](../../proxy/docs/README.md) §6。新增 backoffice API 只改 gateway。

### QEMU Lab 端口

来源：`environments/local-qemu/lab-ports.env`

| 服务 | 容器 | 端口 |
|------|------|------|
| gateway | `simple-living-gateway` | 8080 |
| backoffice-backend | `simple-living-backoffice-backend` | 9110 |
| backoffice-web | `simple-living-backoffice-web` | 8088 |
| proxy | `simple-living-proxy` | 80 |
| foundation | postgres / redis / kafka | 5432 / 6379 / 9092 |

### 启动顺序

```bash
bash services/foundation/deploy/start_nodes.sh
bash services/foundation/deploy/deploy_service.sh up
bash services/platform/backoffice-backend/deploy/deploy_service.sh
bash services/recommendation-server/deploy/deploy_service.sh
bash services/user-server/deploy/deploy_service.sh
bash services/tracking-server/deploy/deploy_service.sh
bash services/gateway/deploy/deploy_service.sh
bash services/platform/backoffice-web/deploy/deploy_service.sh
bash services/proxy/deploy/deploy_service.sh
```

单服务脚本见各目录 `deploy/`；回滚用 `IMAGE_TAG=<tag> deploy_service.sh`。

### 关键配置

**backoffice-web**：Node 20 + Vite，`base=/backoffice/`，容器 nginx :8088。本地 `npm run dev` → `http://127.0.0.1:5173/backoffice/`。默认访问令牌见 gateway `-backoffice_api_token`（`simple-living-ops`）。

**backoffice-backend**：`-port=9110`，`-pg_conninfo`（必填），`-export_dir`，`-secret_backend_uri`（affiliate，后续接入）。

**gateway**：`-backoffice_api_token=simple-living-ops`（运营 Bearer；空值则关闭鉴权，仅联调）。

### 安全与可观测

- 入口限流 + 生产 HTTPS（`proxy` 模板）；gateway 校验运营角色 `Authorization: Bearer`。
- 健康：`GET /backoffice/`、`GET /healthz`、gateway `/api/v2/health/check`、backoffice-backend brpc HealthCheck。
- 写操作对账：`meta.request_id` / `meta.trace_id` 与 gateway、后端日志对齐。

更多 runbook：[`proxy/docs/README.md`](../../proxy/docs/README.md)、[`environments/local-qemu/README.md`](../../../environments/local-qemu/README.md)。

## 文档索引

| 文档 | 说明 |
|------|------|
| [product-spec.md](./product-spec.md) | **产品规格**：目标、三域能力、角色、状态机、范围 |
| [detail-design.md](./detail-design.md) | **详细设计**：页面、字段与操作、API 映射、写链路、代码索引 |
| [backend-development.md](./backend-development.md) | **后端工程**：Bazel、运行配置、部署检查 |
| [backend-api.md](./backend-api.md) | **后端 RPC**：模块、RPC 映射、错误码 |
| [backend-data-model.md](./backend-data-model.md) | **后端数据**：schema、导出 bundle、迁移 |
| [backend-workflow.md](./backend-workflow.md) | **后端工作流**：写链路、导出发布、领域事件 |

工程入口：

| 目录 | 说明 |
|------|------|
| `backoffice-web/` | [README.md](../backoffice-web/README.md)、[deploy/README.md](../backoffice-web/deploy/README.md) |
| `backoffice-backend/` | [deploy/README.md](../backoffice-backend/deploy/README.md) |

## 当前阶段

**阶段一**：`guide_card` 全生命周期、联盟增查、治理审核与可见性裁决、操作日志；公网 `/backoffice/` 同源访问。

**阶段二**：运营登录、全资源类型编辑、审计落库、素材库。详见 [product-spec.md](./product-spec.md) §5。
