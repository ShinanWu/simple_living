# platform 服务组

`services/platform/` 是**运营管理后台**总目录（非 C 端 App）。

## 组成

| 目录 | 说明 |
|------|------|
| [`docs/`](docs/README.md) | 产品与 design（产品规格、详细设计、后端工程文档） |
| [`backoffice-web/`](backoffice-web/) | UI：运营工作台 React SPA |
| [`backoffice-backend/`](backoffice-backend/) | 后端：content + governance + affiliate 写面 + 导出 |

## 文档从哪读

1. [docs/README.md](docs/README.md) — 总览、**架构与部署**、文档索引  
2. [docs/product-spec.md](docs/product-spec.md) — 做什么  
3. [docs/detail-design.md](docs/detail-design.md) — 怎么做（页面、API、写链路）  
4. [docs/backend-development.md](docs/backend-development.md) — 后端构建、运行与部署  
5. [docs/backend-api.md](docs/backend-api.md) / [backend-data-model.md](docs/backend-data-model.md) / [backend-workflow.md](docs/backend-workflow.md) — RPC、DDL、导出工作流

子目录 README 仅描述工程入口（构建、部署脚本）。

## 边界

- 与 `client/` 无代码耦合；运营契约见 `gateway/docs/backoffice-backend.md` 与 `platform/docs/`。
- UI 只经 gateway 调 API；后端导出 snapshot 供 `recommendation-server` / `tracking-server` 消费。

C 端导购读 + 推荐：`services/recommendation-server/`。
