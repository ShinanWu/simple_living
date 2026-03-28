# Gateway 页面 / BFF 聚合约定

## 1. 目的

部分客户端屏幕需要 **多域只读数据在同一屏展示**（例如：推荐流 + 卡片详情摘要 + 治理披露字段）。若在客户端串行请求，延迟与弱网体验差；若让每个域各自暴露「大而全」接口，则边界混乱。

**BFF（Backend for Frontend）聚合由 gateway 承担**：在 **读路径** 上并行调用多个内部 RPC，将结果组装为 **页面导向（screen-oriented）** 的 JSON，仍使用统一响应信封与 `snake_case` 字段。

## 2. 原则

| 原则 | 说明 |
|------|------|
| **聚合非所有权** | 聚合响应中的业务含义仍来自各域；gateway 不新增业务规则，只做编排、并行、字段拼装与对外裁剪 |
| **只读优先** | 默认页面接口为 GET 或 POST-body 只读查询；写操作保持单域单事务，不经由复杂聚合 |
| **契约先行** | 每个页面接口必须在本文档登记：路径、下游 RPC 列表、`data` 形状引用（contracts 或内嵌表格） |
| **失败策略显式** | 部分依赖失败时，选择「整页失败」或「降级字段为 null/空列表」须在接口级写死，且 `code`/`warnings` 行为符合 [common-response.md](../../../docs/contracts/common-response.md) |
| **与公共契约一致** | 导购卡片、推荐条目、跳转字段须符合 [guide-card.md](../../../docs/contracts/guide-card.md)、[recommendation.md](../../../docs/contracts/recommendation.md)、[redirect-attribution.md](../../../docs/contracts/redirect-attribution.md) |

## 3. 首批页面接口

下列为当前首批页面接口的推荐命名与依赖关系，供并行开发对齐；**真实对外路径、版本与 `data` 结构以 `api.md` 为准**，本文只补充聚合关系。后续新增页面接口须继续在本表登记并记入 [changelog.md](./changelog.md)。

| 页面 / 场景 | 对外路径（以 `api.md` 为准） | 下游 RPC（逻辑名） | `data` 要点 |
|-------------|------------------|---------------------|-------------|
| 首页推荐流 | `GET /api/v2/pages/home_feed` | `RecommendationService/QueryRecommendations` + `ContentService/BatchGetGuideCards` + 可选治理可见性过滤 | `items[]` 引用推荐契约 + 卡片 ID；卡片富展示可内嵌 [guide-card](../../../docs/contracts/guide-card.md) 子集 |
| 导购详情页 | `GET /api/v2/pages/guide_detail` | `ContentService/BatchGetGuideCards` + `GovernanceCooperationService/BatchGetCooperationLabels` + 可选 `RecommendationService/QueryRecommendations` | `guide`、`disclosures`、`related[]` 分区；禁止泄漏内部审核状态细节 |
| 跳转准备页 | `POST /api/v2/pages/redirect_prepare` | `TrackingLinkService/AssembleTrackingLink` | 返回可点击 URL、click_id 等，字段语义见 [redirect-attribution.md](../../../docs/contracts/redirect-attribution.md) |
| 「我的」摘要 | `GET /api/v2/pages/me_summary` | `UserDomainService/GetMeSummary` | `user_id` / `is_guest` 与 [auth.md](../../../docs/contracts/auth.md) 可选摘要一致 |

**说明**：逻辑 RPC 名与 `services/*/proto/*.proto` 对齐；若本文与 `api.md` 不一致，以 `api.md` 为准。gateway 负责 **并行调用** 与 **超时预算**（见 [workflow.md](./workflow.md)）。

## 4. 聚合响应形状约定

- 顶层仍为标准信封；页面专属负载放在 `data` 内。
- 建议 `data` 顶层按 **稳定分区键** 组织，例如：`feed`、`pagination`、`guide`、`affiliate`、`tracking`，避免深层随意嵌套导致端上解析分裂。
- 列表分页：统一使用 [pagination.md](../../../docs/contracts/pagination.md) 的 `data.items` + `data.pagination`（若该页带列表）。

## 5. 与细粒度 API 的关系

- **单资源、单域** 操作：优先走 [api.md](./api.md) 中的资源路由，便于缓存与权限边界清晰。
- **多域只读拼屏**：使用 `api.md` 中已登记的页面路由，本文档负责解释聚合关系与失败策略，避免客户端多次往返。

## 6. 客户端并行开发

客户端可依据：

1. 本文档路径与 `data` 分区；
2. 公共契约中的卡片/推荐/跳转结构；
3. `meta.request_id` 排障。

Mock 数据应模拟 **部分下游失败** 场景，验证降级策略与 `warnings`。

## 7. 非目标

- 不在此层做跨域 **分布式写事务**。
- 不把聚合接口当作「通用 GraphQL」无限扩展；新增页面接口须评审下游数量与超时预算。
