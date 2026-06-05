# platform 服务组说明

`services/platform/` 承载**平台运营侧**服务（非 C 端用户 App）。

## 当前成员

| 目录 | 说明 |
|------|------|
| `backoffice-web/` | 运营管理后台 Web（React SPA） |
| `backoffice-backend/` | 运营后台后端：content + governance + affiliate 写面权威库 + snapshot 导出 |

## 边界

- 与 `client/` **无代码或文档耦合**；C 端契约与验收在 `client/frontend-*.md`，运营后台仅在 `backoffice-web/docs/` 与 `gateway/docs/backoffice-backend.md` 维护。
- 不放 C 端用户页面。
- `backoffice-web` 只经 `gateway` 调 backoffice API，不直连域服务 proto。
- `backoffice-backend` 产出导出物供同节点 `recommendation-server`（C 端读）与 `tracking-server` 消费；**不**承接 C 端高频读 RPC。

## C 端相关服务位置

C 端导购读 + 推荐：`services/recommendation-server/`（不在 `platform/` 下）。
