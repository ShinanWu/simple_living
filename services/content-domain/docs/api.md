# content-domain — 内部 RPC 契约（proto2）

> 本文档为 `content-domain` **内部** brpc/gRPC 风格 RPC 的单一事实来源：含完整 RPC 列表、请求/响应消息、嵌套类型与枚举。客户端对外 JSON 由 `gateway` 定义并映射；本域不暴露本 `proto` 形状给 C 端。  
> 假定调用链：**客户端 → gateway（HTTPS/JSON）→ content-domain（内部 RPC）**。

---

## 1. 通用约定

### 1.1 身份与可见性

- **身份**：读接口支持匿名与登录态；登录态仅用于网关透传用户 ID，**本域不实现用户画像**。
- **可见性（v1）**：对用户是否可见以 **`governance-domain` 的可见性裁决为最终准入**；本域的 `content_status` 表示编辑/生命周期态，二者都需满足时 reader 接口才返回实体。不可见实体不返回或按错误语义处理（见 §11）。
- **幂等**：写接口须携带 `client_request_id`（ULID/UUID 字符串）；相同键重复提交返回同一业务结果（409/幂等冲突码在实现期与 `error-codes.md` 对齐）。
- **乐观锁**：可更新资源携带 `revision`（int64）；`expected_revision` 不匹配时返回版本冲突码（见 §11）。

### 1.2 分页（游标）

列表类读接口统一采用 **游标分页**，语义对齐 `docs/contracts/pagination.md`：

| 位置 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 请求 | `cursor` | string | 可选；上一页响应的 `pagination.next_cursor`；首页省略或空 |
| 请求 | `limit` | int32 | 可选；本页最大条数，**默认与上限 100**（接口可声明更小上限） |
| 响应 | `pagination.next_cursor` | string \| null | 下一页游标；无更多时为 `null` |
| 响应 | `pagination.has_more` | bool | 是否还有下一页 |
| 响应 | `pagination.limit` | int32 | 本页请求的最大条数（回显） |

**空列表**：业务数组为空、`has_more = false`、`next_cursor = null`；**不**用“资源不存在”类错误表示空列表。

### 1.3 字段裁剪

`BatchGetGuideCards` 等接口可使用 Protobuf `google.protobuf.FieldMask paths` 指定返回字段子集；未列出的字段实现可省略（置默认/空）。路径与 `GuideCard` 等消息字段名一致（snake_case）。

### 1.4 时间类型

文中 **timestamp** 在 `proto` 中为 `google.protobuf.Timestamp`；文档表格中记为 RFC3339 / 瞬时时间点语义。

---

## 2. 枚举定义

### 2.1 `LifeTheme`

| 名称 | 说明 |
|------|------|
| `LIFE_THEME_CLOTHING` | 衣 |
| `LIFE_THEME_FOOD` | 食 |
| `LIFE_THEME_HOUSING` | 住 |
| `LIFE_THEME_MOBILITY` | 行 |

### 2.2 `ThemeStatus`

| 名称 | 说明 |
|------|------|
| `THEME_STATUS_ACTIVE` | 启用 |
| `THEME_STATUS_INACTIVE` | 停用（默认读接口不返回，除非 `include_inactive`） |

### 2.3 `ContentLifecycleStatus`

内容在**内容域 + 治理联动**下的生命周期（与治理审核态可拆分或映射）。

| 名称 | 说明 |
|------|------|
| `CONTENT_LIFECYCLE_STATUS_DRAFT` | 草稿，对公网读者不可见 |
| `CONTENT_LIFECYCLE_STATUS_IN_REVIEW` | 已提交审核 |
| `CONTENT_LIFECYCLE_STATUS_PUBLISHED` | 已发布（**仍需治理可见性 `published` 才对用户展示**） |
| `CONTENT_LIFECYCLE_STATUS_SCHEDULED` | 已批准，未到生效时间 |
| `CONTENT_LIFECYCLE_STATUS_OFFLINE` | 运营或治理下架 |
| `CONTENT_LIFECYCLE_STATUS_ARCHIVED` | 归档，仅内部查询 |

### 2.4 `GuideCardType`

| 名称 | 说明 |
|------|------|
| `GUIDE_CARD_TYPE_PHYSICAL_GOOD` | 实物商品导购 |
| `GUIDE_CARD_TYPE_DIGITAL_GOOD` | 虚拟/数字商品 |
| `GUIDE_CARD_TYPE_LOCAL_SERVICE` | 本地生活服务 |
| `GUIDE_CARD_TYPE_OTHER` | 其他（扩展保留） |

### 2.5 `MediaType`

| 名称 | 说明 |
|------|------|
| `MEDIA_TYPE_IMAGE` | 图片 |
| `MEDIA_TYPE_VIDEO` | 视频 |
| `MEDIA_TYPE_ICON` | 小图标 |

### 2.6 `EditorialBlockType`

| 名称 | 说明 |
|------|------|
| `EDITORIAL_BLOCK_TYPE_PARAGRAPH` | 富文本段落 |
| `EDITORIAL_BLOCK_TYPE_HEADING` | 标题 |
| `EDITORIAL_BLOCK_TYPE_IMAGE` | 配图 |
| `EDITORIAL_BLOCK_TYPE_EMBED_CARD` | 嵌入导购卡片 |
| `EDITORIAL_BLOCK_TYPE_QUOTE` | 引用 |
| `EDITORIAL_BLOCK_TYPE_BULLET_LIST` | 列表 |

### 2.7 `TopicModuleType`

| 名称 | 说明 |
|------|------|
| `TOPIC_MODULE_TYPE_CARD_GRID` | 卡片网格 |
| `TOPIC_MODULE_TYPE_CARD_CAROUSEL` | 横滑卡片 |
| `TOPIC_MODULE_TYPE_EDITORIAL_LIST` | 图文列表 |
| `TOPIC_MODULE_TYPE_RANKING_EMBED` | 嵌入整块榜单 |
| `TOPIC_MODULE_TYPE_CUSTOM_BANNER` | 头图/运营横幅位 |

### 2.8 `RankingUpdateCadence`（展示用）

| 名称 | 说明 |
|------|------|
| `RANKING_UPDATE_CADENCE_REALTIME` | 文案宣称实时 |
| `RANKING_UPDATE_CADENCE_DAILY` | 日更 |
| `RANKING_UPDATE_CADENCE_WEEKLY` | 周更 |
| `RANKING_UPDATE_CADENCE_MANUAL` | 人工不定期 |

### 2.9 `GuideCardListSort`

| 名称 | 说明 |
|------|------|
| `GUIDE_CARD_LIST_SORT_NEWEST` | 最新上架/更新时间 |
| `GUIDE_CARD_LIST_SORT_MANUAL_RANK` | 运营手工排序 |

### 2.10 `ContentResourceKind`（写路径 / 审核关联）

| 名称 | 说明 |
|------|------|
| `CONTENT_RESOURCE_KIND_GUIDE_CARD` | 导购卡片 |
| `CONTENT_RESOURCE_KIND_EDITORIAL_CONTENT` | 攻略/图文 |
| `CONTENT_RESOURCE_KIND_TOPIC` | 专题 |
| `CONTENT_RESOURCE_KIND_RANKING_LIST` | 榜单 |

---

## 3. 通用消息

### 3.1 `CursorRequest`（嵌入列表请求）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cursor` | string | 否 | 游标 |
| `limit` | int32 | 否 | 条数上限 |

### 3.2 `PaginationCursor`（嵌入列表响应）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `next_cursor` | string | 否 | 无下一页时省略或空串；`proto2` 可用 `optional` |
| `has_more` | bool | 是 | |
| `limit` | int32 | 是 | 回显 |

### 3.3 `MediaRef`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `media_id` | string | 否 | 素材库 ID |
| `url` | string | 是 | CDN 地址 |
| `type` | `MediaType` | 是 | |
| `width` | int32 | 否 | |
| `height` | int32 | 否 | |

### 3.4 `AffiliateRef`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `channel` | string | 是 | 如 `PDD` / `DOUYIN`，与 `affiliate-domain` 字典对齐 |
| `external_item_id` | string | 是 | 渠道侧商品或套餐 ID（opaque） |
| `external_shop_id` | string | 否 | 店铺 ID |
| `payload` | map<string, string> | 否 | 扩展；**本域不解析转链规则** |

### 3.5 `Tag`（轻量标签）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `tag_id` | string | 是 | |
| `name` | string | 是 | |
| `theme_ids` | repeated string | 否 | 与主题关联 |

---

## 4. 主题

### 4.1 `ThemeSummary`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_id` | string | 是 | |
| `parent_id` | string | 否 | |
| `life_theme` | `LifeTheme` | 是 | 衣食住行大类 |
| `slug` | string | 是 | URL / 埋点键 |
| `display_name` | string | 是 | |
| `description` | string | 否 | |
| `icon_url` | string | 否 | |
| `sort_order` | int32 | 是 | 同级排序 |
| `status` | `ThemeStatus` | 是 | |

### 4.2 `BannerRef`（运营位占位）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `ref_type` | string | 是 | 如 `card` / `topic` / `content` / `external` |
| `ref_id` | string | 否 | 非外链时实体 ID |
| `landing_url` | string | 否 | 外链时落地 URL |

### 4.3 RPC：`ListThemes`

| 方向 | 消息 | 说明 |
|------|------|------|
| 请求 | `ListThemesRequest` | |
| 响应 | `ListThemesResponse` | |

**`ListThemesRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `locale` | string | 否 | 多端文案 |
| `include_inactive` | bool | 否 | 默认 `false`；仅运营角色时由网关鉴权 |

**`ListThemesResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `themes` | repeated `ThemeSummary` | 是 | 树可拍平为列表，靠 `parent_id` 还原 |
| `etag` | string | 否 | 缓存失效 |
| `version` | int64 | 否 | 配置版本号 |

### 4.4 RPC：`GetThemeDetail`

**`GetThemeDetailRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_id` | string | 是 | |

**`GetThemeDetailResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme` | `ThemeSummary` | 是 | 与摘要同形，可含更多运营字段 |
| `banner_refs` | repeated `BannerRef` | 否 | 专题/内容/外链占位，跳转由 gateway 解析 |

---

## 5. 导购卡片

### 5.1 `GuideCard`（完整）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `card_id` | string | 是 | |
| `type` | `GuideCardType` | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `cover_media` | `MediaRef` | 是 | |
| `media_gallery` | repeated `MediaRef` | 否 | |
| `selling_points` | repeated string | 否 | |
| `price_hint` | string | 否 | 展示价文案，非成交价 |
| `theme_ids` | repeated string | 否 | |
| `tag_ids` | repeated string | 否 | |
| `affiliate_refs` | repeated `AffiliateRef` | 否 | opaque，供 tracking/affiliate |
| `commercial_disclosure_required` | bool | 是 | 是否与治理披露规则一致 |
| `content_status` | `ContentLifecycleStatus` | 是 | |
| `published_revision` | int64 | 是 | 当前对外版本号 |
| `effective_from` | timestamp | 否 | |
| `effective_to` | timestamp | 否 | |
| `revision` | int64 | 否 | 乐观锁（写路径） |
| `created_at` / `updated_at` | timestamp | 否 | 审计 |
| `created_by` / `updated_by` | string | 否 | 运营主体 ID |

### 5.2 `GuideCardSummary`（列表/批量裁剪）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `card_id` | string | 是 | |
| `type` | `GuideCardType` | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `cover_media` | `MediaRef` | 否 | |
| `theme_ids` | repeated string | 否 | |
| `price_hint` | string | 否 | |
| `content_status` | `ContentLifecycleStatus` | 是 | |
| `commercial_disclosure_required` | bool | 是 | |

### 5.3 RPC：`BatchGetGuideCards`

**`BatchGetGuideCardsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `card_ids` | repeated string | 是 | 建议 ≤50，与网关约定 |
| `field_mask` | `FieldMask` | 否 | 裁剪字段；未指定则返回完整 `GuideCard` |

**`BatchGetGuideCardsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cards` | repeated `GuideCard` | 是 | 与 mask 一致；找不到的 ID 不出现在此数组 |
| `missing_ids` | repeated string | 是 | 不存在、无权限或不可见的 ID |

### 5.4 RPC：`ListGuideCards`

**`ListGuideCardsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_id` | string | 否 | |
| `tag_ids` | repeated string | 否 | |
| `sort` | `GuideCardListSort` | 否 | 默认 `NEWEST` |
| `cursor` | string | 否 | |
| `limit` | int32 | 否 | |

**`ListGuideCardsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cards` | repeated `GuideCardSummary` | 是 | 实现可选用全量 `GuideCard` |
| `pagination` | `PaginationCursor` | 是 | |

---

## 6. 攻略与图文

### 6.1 `EditorialBlock`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `block_id` | string | 是 | |
| `type` | `EditorialBlockType` | 是 | |
| `text` | string | 否 | 文本类 |
| `media` | `MediaRef` | 否 | 媒体类 |
| `card_id` | string | 否 | `EMBED_CARD` 时 |

### 6.2 `EditorialContent`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `title` | string | 是 | |
| `summary` | string | 否 | 列表摘要 |
| `cover` | `MediaRef` | 否 | |
| `author_display_name` | string | 否 | |
| `theme_ids` | repeated string | 否 | |
| `tag_ids` | repeated string | 否 | |
| `blocks` | repeated `EditorialBlock` | 是 | |
| `content_status` | `ContentLifecycleStatus` | 是 | |
| `published_at` | timestamp | 否 | |
| `revision` | int64 | 否 | 乐观锁 |
| `created_at` / `updated_at` | timestamp | 否 | |

### 6.3 `EditorialContentListItem`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |
| `title` | string | 是 | |
| `summary` | string | 否 | |
| `cover` | `MediaRef` | 否 | |
| `published_at` | timestamp | 否 | |

### 6.4 RPC：`GetEditorialContent`

**`GetEditorialContentRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_id` | string | 是 | |

**`GetEditorialContentResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content` | `EditorialContent` | 是 | |
| `embedded_card_ids` | repeated string | 是 | 从块中抽取的卡片引用（可冗余便于调用方） |

### 6.5 RPC：`ListEditorialContents`

**`ListEditorialContentsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_id` | string | 否 | 与 `collection_id` 可组合，以实现为准 |
| `collection_id` | string | 否 | 专题/集合 ID（若产品拆分） |
| `cursor` | string | 否 | |
| `limit` | int32 | 否 | |

**`ListEditorialContentsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `items` | repeated `EditorialContentListItem` | 是 | |
| `pagination` | `PaginationCursor` | 是 | |

---

## 7. 专题（Topic）

### 7.1 `TopicModuleItem`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `item_id` | string | 是 | 行 ID |
| `card_id` | string | 否 | |
| `content_id` | string | 否 | |
| `ranking_id` | string | 否 | |
| `external_url` | string | 否 | 外链占位 |
| `caption` | string | 否 | |
| `sort_order` | int32 | 是 | 模块内顺序 |

（业务约束：每条目至少一种引用非空；冲突时以实现/校验为准。）

### 7.2 `TopicModule`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `module_id` | string | 是 | |
| `type` | `TopicModuleType` | 是 | |
| `title` | string | 否 | |
| `sort_order` | int32 | 是 | |
| `items` | repeated `TopicModuleItem` | 是 | |

### 7.3 `Topic`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `topic_id` | string | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `banner` | `MediaRef` | 否 | |
| `theme_ids` | repeated string | 否 | |
| `modules` | repeated `TopicModule` | 是 | |
| `content_status` | `ContentLifecycleStatus` | 是 | |
| `seo_slug` | string | 否 | |
| `revision` | int64 | 否 | |

### 7.4 `TopicSummary`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `topic_id` | string | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `banner` | `MediaRef` | 否 | |
| `theme_ids` | repeated string | 否 | |
| `content_status` | `ContentLifecycleStatus` | 是 | |

### 7.5 RPC：`GetTopic`

**`GetTopicRequest`**: `topic_id` (string, 必填)

**`GetTopicResponse`**: `topic` (`Topic`)

### 7.6 RPC：`ListTopics`

**`ListTopicsRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `theme_id` | string | 否 | |
| `content_status` | `ContentLifecycleStatus` | 否 | 读者接口默认仅 `PUBLISHED`；运营读可覆盖 |

**`ListTopicsResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `topics` | repeated `TopicSummary` | 是 | |

（若后续需分页，在不变更语义前提下为请求增加 `CursorRequest` 字段、响应增加 `pagination`。）

---

## 8. 榜单

### 8.1 `RankingEntry`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `entry_id` | string | 是 | |
| `rank` | int32 | 是 | 从 1 开始 |
| `card_id` | string | 否 | |
| `content_id` | string | 否 | |
| `blurb` | string | 否 | 短评 |

### 8.2 `RankingList`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `ranking_id` | string | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `cover` | `MediaRef` | 否 | |
| `theme_ids` | repeated string | 否 | |
| `rule_summary` | string | 是 | 规则一句话 |
| `update_cadence` | `RankingUpdateCadence` | 是 | |
| `entries` | repeated `RankingEntry` | 是 | 有序 |
| `content_status` | `ContentLifecycleStatus` | 是 | |
| `revision` | int64 | 否 | |

### 8.3 `RankingListSummary`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `ranking_id` | string | 是 | |
| `title` | string | 是 | |
| `subtitle` | string | 否 | |
| `cover` | `MediaRef` | 否 | |

### 8.4 RPC：`GetRankingList`

**`GetRankingListRequest`**: `ranking_id` (string, 必填)

**`GetRankingListResponse`**: `ranking` (`RankingList`)

### 8.5 RPC：`ListRankingLists`

**`ListRankingListsRequest`**: `theme_id` (string, 可选)

**`ListRankingListsResponse`**: `rankings` (repeated `RankingListSummary`)

---

## 9. 运营写模型（网关鉴权，不对普通用户开放）

### 9.1 v1 与治理的关系

- **审核通过不自动发布**：`SubmitForReview` 后进入治理队列；**显式 `PublishRevision` 仅在治理可见性允许发布时**才能完成对用户可见侧的切换（具体与 `governance-domain` 裁决一致）。
- **可见性最终门闸**：即使 `content_status = PUBLISHED`，若治理裁决非 `published`，读者接口仍不得暴露（见 §1.1、§11 错误语义）。

### 9.2 RPC：`UpsertGuideCard`

**`UpsertGuideCardRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | 幂等 |
| `expected_revision` | int64 | 否 | 更新必填（或实现约定 `0` 为创建） |
| `guide_card` | `GuideCard` | 是 | `card_id` 空为创建，非空为更新 |

**`UpsertGuideCardResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `card_id` | string | 是 | |
| `revision` | int64 | 是 | 新版本 |

### 9.3 RPC：`UpsertEditorialContent`

**`UpsertEditorialContentRequest`**: `client_request_id`, `expected_revision`, `content` (`EditorialContent`)

**`UpsertEditorialContentResponse`**: `content_id`, `revision`

### 9.4 RPC：`UpsertTopic`

**`UpsertTopicRequest`**: `client_request_id`, `expected_revision`, `topic` (`Topic`)

**`UpsertTopicResponse`**: `topic_id`, `revision`

### 9.5 RPC：`UpsertRankingList`

**`UpsertRankingListRequest`**: `client_request_id`, `expected_revision`, `ranking` (`RankingList`)

**`UpsertRankingListResponse`**: `ranking_id`, `revision`

### 9.6 RPC：`SubmitForReview`

**`SubmitForReviewRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `resource_kind` | `ContentResourceKind` | 是 | |
| `resource_id` | string | 是 | `card_id` / `content_id` / `topic_id` / `ranking_id` |
| `revision` | int64 | 是 | 提交审核的版本 |

**`SubmitForReviewResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_status` | `ContentLifecycleStatus` | 是 | 置为 `IN_REVIEW`（或实现等价态） |
| `governance_queue_item_id` | string | 否 | 若同步入队成功则返回 |

### 9.7 RPC：`PublishRevision`

**`PublishRevisionRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `client_request_id` | string | 是 | |
| `resource_kind` | `ContentResourceKind` | 是 | |
| `resource_id` | string | 是 | |
| `revision` | int64 | 是 | 要发布的版本 |

**`PublishRevisionResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `content_status` | `ContentLifecycleStatus` | 是 | 成功时一般为 `PUBLISHED` 或 `SCHEDULED` |
| `published_revision` | int64 | 是 | |

若治理未放行或版本不匹配，返回业务错误码（见 §11）。

---

## 10. 服务定义：`ContentService`

| RPC | 请求 | 响应 |
|-----|------|------|
| `ListThemes` | `ListThemesRequest` | `ListThemesResponse` |
| `GetThemeDetail` | `GetThemeDetailRequest` | `GetThemeDetailResponse` |
| `BatchGetGuideCards` | `BatchGetGuideCardsRequest` | `BatchGetGuideCardsResponse` |
| `ListGuideCards` | `ListGuideCardsRequest` | `ListGuideCardsResponse` |
| `GetEditorialContent` | `GetEditorialContentRequest` | `GetEditorialContentResponse` |
| `ListEditorialContents` | `ListEditorialContentsRequest` | `ListEditorialContentsResponse` |
| `GetTopic` | `GetTopicRequest` | `GetTopicResponse` |
| `ListTopics` | `ListTopicsRequest` | `ListTopicsResponse` |
| `GetRankingList` | `GetRankingListRequest` | `GetRankingListResponse` |
| `ListRankingLists` | `ListRankingListsRequest` | `ListRankingListsResponse` |
| `UpsertGuideCard` | `UpsertGuideCardRequest` | `UpsertGuideCardResponse` |
| `UpsertEditorialContent` | `UpsertEditorialContentRequest` | `UpsertEditorialContentResponse` |
| `UpsertTopic` | `UpsertTopicRequest` | `UpsertTopicResponse` |
| `UpsertRankingList` | `UpsertRankingListRequest` | `UpsertRankingListResponse` |
| `SubmitForReview` | `SubmitForReviewRequest` | `SubmitForReviewResponse` |
| `PublishRevision` | `PublishRevisionRequest` | `PublishRevisionResponse` |

---

## 11. 错误语义（与 `docs/contracts/error-codes.md` 对齐）

内部 RPC **成功** 返回上述消息；**失败** 使用 RPC 层非 OK 状态，并在应用错误详情（若使用）中携带整数 `code`，与下列语义一致（节选；完整表以契约为准）：

| code | 语义 |
|------|------|
| `0` | 成功 |
| `10001` | 缺少必填参数 |
| `10002` | 参数校验失败 |
| `20004` | 权限不足（运营接口） |
| `30001` | 资源不存在 |
| `30002` | 资源已下架或不可见 |
| `30003` | 资源未发布 |
| `60001` | 合规拦截（治理不可展示） |
| `60002` | 地域或策略限制 |
| `90001`–`90003` | 系统/依赖类 |

**版本冲突**（乐观锁）：建议映射为 `10002` 子类或域内预留码，定稿后写入 `changelog.md` 与 `error-codes.md`。

**幂等重放**：与成功首次结果一致返回 `0`；冲突时返回可区分码（实现期固定）。

**Fail 策略**：依赖治理/下游异常时，读者路径默认 **fail-closed**（不返回应隐藏内容）。

---

## 12. ContentReadFacade（跨域只读门面）

> 本节定义 `content-domain` 对外暴露的**最小稳定只读接口子集**，供 `governance-domain` 等跨域消费方在不依赖完整 `ContentService` 的前提下获取内容摘要。  
> 解耦背景见 [docs/contracts/domain-events.md](../../../docs/contracts/domain-events.md)。

### 12.1 设计原则

- **只读**：Facade 不包含任何写操作。
- **最小表面**：仅暴露消费方实际需要的摘要字段，不返回完整实体。
- **稳定优先**：Facade 接口变更频率应远低于完整 ContentService；破坏性变更须同步 governance-domain changelog。
- **独立可 mock**：governance-domain 开发时可仅 mock 此 Facade，不必启动完整 content-domain。

### 12.2 `ContentMetaSummary`（门面摘要消息）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `resource_id` | string | 是 | 实体 ID |
| `resource_kind` | `ContentResourceKind` | 是 | 卡片 / 图文 / 专题 / 榜单 |
| `title` | string | 是 | 标题 |
| `theme_ids` | repeated string | 否 | 主题 ID 列表 |
| `content_status` | `ContentLifecycleStatus` | 是 | 当前编辑生命周期状态 |
| `revision` | int64 | 是 | 当前版本号 |
| `commercial_disclosure_required` | bool | 否 | 是否需要商业披露 |
| `updated_at` | timestamp | 否 | 最近更新时间 |

### 12.3 RPC：`BatchGetContentMetaSummary`

**`BatchGetContentMetaSummaryRequest`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `resource_ids` | repeated string | 是 | 批量上限 50 |
| `resource_kind` | `ContentResourceKind` | 否 | 若指定则仅返回该类型；未指定则按 ID 自动识别 |

**`BatchGetContentMetaSummaryResponse`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `summaries` | repeated `ContentMetaSummary` | 是 | |
| `missing_ids` | repeated string | 是 | 不存在或无权限的 ID |

### 12.4 服务定义：`ContentReadFacade`

| RPC | 请求 | 响应 |
|-----|------|------|
| `BatchGetContentMetaSummary` | `BatchGetContentMetaSummaryRequest` | `BatchGetContentMetaSummaryResponse` |

此 Facade 作为独立 `service` 块，与 `ContentService` 分离，便于消费方仅依赖门面 proto 而不引入完整写接口。

---

## 13. `proto` 映射

本文件与 `services/content-domain/proto/content_domain.proto` 对齐；字段名 **snake_case**，枚举名以 `proto` 为准。

新增 `ContentReadFacade` service 块，详见 §12。
