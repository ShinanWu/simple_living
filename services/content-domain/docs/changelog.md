# content-domain 变更记录

## v1.0.0 — 2026-03-28

- **初始文档**：建立 `content-domain` 服务文档集（`README.md`、`api.md`、`pages.md`、`data-model.md`、`workflow.md`、`changelog.md`）。
- **边界**：明确本域负责导购内容、专题、榜单、商品/服务卡片与衣食住行主题管理；不包含用户画像、推荐排序、联盟转链、归因与完整审核引擎。
- **API 草案**：定义主题、导购卡片、图文、专题、榜单的读接口及运营写接口清单；具体 RPC 名与错误码以实现阶段 + `docs/contracts/` 为准。
- **数据模型**：定义 `GuideCard`、`EditorialContent`、`Topic`、`RankingList`、`Theme` 等核心实体与主要枚举。
- **页面映射**：描述首页、主题频道、详情、专题、榜单与收藏聚合场景下内容域的参与方式。
