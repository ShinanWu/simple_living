# backoffice-backend — 页面映射

本服务无直连客户端；页面消费均经 `gateway` → `platform/backoffice-web`。

| 页面/模块 | gateway 路由 | 本服务模块 |
|-----------|--------------|------------|
| 内容列表/编辑 | `/api/v2/backoffice/content/*` | content |
| 审核队列 | `/api/v2/backoffice/governance/reviews*` | governance |
| 可见性裁决 | `/api/v2/backoffice/governance/visibility` | governance |
| 联盟伙伴 | `/api/v2/backoffice/affiliate/*` | affiliate |

C 端页面（首页、详情、转链）不调用本服务；见 `services/recommendation-server/docs/pages.md` 与 `services/tracking-server/docs/pages.md`。
