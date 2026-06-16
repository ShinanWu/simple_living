# 运营管理平台

`services/platform/` 是**运营管理后台**总目录：导购内容、治理审核、联盟配置；写面权威在 `backoffice-backend`，经 snapshot 同步 C 端读路径。

## 组成

| 目录 | 角色 | 职责 |
|------|------|------|
| [`backoffice-web/`](./backoffice-web/) | UI | 运营工作台（React SPA）；经 gateway 调 API |
| [`backoffice-backend/`](./backoffice-backend/README.md) | 后端 | content + governance + affiliate 写面；PostgreSQL + 导出 |

## 契约

| 文档 | 说明 |
|------|------|
| [api.md](./api.md) | 后端内部 RPC（brpc/proto） |
| [backoffice-gateway-api.md](./backoffice-gateway-api.md) | 运营 HTTP（`/api/v2/backoffice/*`） |
| [backoffice-delivery-spec.md](./backoffice-delivery-spec.md) | C 端 snapshot 数据交付与验收 |
| [product-spec.md](./product-spec.md) | 产品规格 |
| [detail-design.md](./detail-design.md) | 页面与写链路设计 |

终端 JSON 信封：[`services/gateway/api.md`](../gateway/api.md)。

## 架构

```text
运营浏览器 → proxy → backoffice-web :8088（/backoffice/）
                    → gateway :8080（/api/）→ backoffice-backend :9110 → PostgreSQL
                                                      ↓ export
                              recommendation-server / tracking-server（同节点 snapshot）
```

| 导出 bundle | 消费方 |
|-------------|--------|
| `catalog_snapshot`、`visibility_index` | recommendation-server |
| `affiliate_link_spec` | tracking-server |

**硬约束**：`backoffice-backend` `-export_dir` 与 `recommendation-server` / `tracking-server` `-snapshot_dir` 同挂载（默认 `/var/lib/simple-living/exports/`）。

## 部署

| URL | 目标 |
|-----|------|
| `/backoffice/` | backoffice-web |
| `/api/v2/backoffice/*` | gateway → backoffice-backend |

启动顺序与端口见 [`environments/local-qemu/README.md`](../../environments/local-qemu/README.md)；单服务脚本在各子目录 `deploy/`。

路由约定：[`services/proxy/README.md`](../proxy/README.md) §6。

## 工程入口

- Web：[backoffice-web/README.md](./backoffice-web/README.md)
- 后端：[backoffice-backend/deploy/README.md](./backoffice-backend/deploy/README.md)

与 `client/` 无代码耦合。C 端导购读见 `services/recommendation-server/`。
