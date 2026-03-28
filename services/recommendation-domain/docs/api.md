# recommendation-domain — 内部 RPC / 逻辑接口（proto2）

本文件是 `recommendation-domain` **内部 gRPC / proto 契约**的单一交付说明：`gateway` 或其他内部消费者实现集成时，**无需再读** `data-model.md` 即可理解场景、上下文、过滤、策略、条目、分页与解释载荷。

对外客户端 HTTP/JSON 由 `gateway` 映射；共享结果字段语义以 [docs/contracts/recommendation.md](../../../docs/contracts/recommendation.md) 与 [docs/contracts/pagination.md](../../../docs/contracts/pagination.md) 为准。JSON 侧字段名使用 **snake_case**（如 `recommendation_id`、`guide_card_id`、`next_cursor`）。

**Proto 源码路径**：`services/recommendation-domain/proto/`（`recommendation_domain_models.proto` 类型与枚举，`recommendation_domain_service.proto` 服务与 RPC 报文）。

---

## 统一约定

| 项 | 约定 |
|----|------|
| 传输 | `proto2` + gRPC（本域标准包名 `simple_living.recommendation_domain`） |
| 时间 | 报文中时间戳字段为 **ISO 8601 UTC 字符串**，除非另行引入 `google.protobuf.Timestamp` |
| 错误 | 使用 **整数业务错误码**（见本文第 8 节）；gRPC 层可配合 `google.rpc.Status` / `details` 携带同一枚举 |
| 敏感数据 | 上下文不传原始 PII；信号优先 `signal_bundle_ref` |
| 排序 | `rank` 为 **从 1 开始** 的整数，与 `guide_card_id` 一起用于归因与 Explain 对齐 |

---

## 1. 场景 `scene`（稳定字符串 + proto 枚举）

逻辑与网关侧 `scene` 为 **稳定 snake_case 字符串**；proto 使用 `RecommendationScene` 枚举，**一一对应**如下表（新增场景须追加枚举值并更新本文与 contracts）：

| `scene` 字符串 | `RecommendationScene`（proto） | 说明 |
|----------------|-------------------------------|------|
| `home_feed` | `HOME_FEED` | 首页个性化混排流 |
| `home_popular` | `HOME_POPULAR` | 首页热门 / 趋势模块 |
| `theme_feed` | `THEME_FEED` | 主题频道列表 |
| `guide_detail_related` | `GUIDE_DETAIL_RELATED` | 导购详情页相关推荐 |
| `search` | `SEARCH` | 搜索场景穿插（可与 `request_context_echo` 区分子场景） |
| `cold_start` | `COLD_START` | 冷启动 / 新用户默认流 |

`RECOMMENDATION_SCENE_UNSPECIFIED`（0）禁止作为有效请求值；应返回参数错误。

---

## 2. 公共嵌套类型

### 2.1 `RecommendationRequestContext`

**用途**：描述请求的 **主体、环境、合规信号**，供召回、排序与解释策略使用。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `user_id` | string | 否 | 已认证用户 ID |
| `session_id` | string | 否 | 匿名或设备绑定会话 |
| `locale` | string | 是 | 语言 / 区域，影响文案与候选集 |
| `channel` | string | 是 | 端类型：应用商店、小程序、Web 等 |
| `app_version` | string | 否 | 客户端版本，用于能力开关 |
| `device_tier` | string | 否 | 粗粒度性能档位提示 |
| `consent` | `ConsentFlags` | 是 | 来自 `user-domain` 的同意标记 |
| `signal_bundle_ref` | string | 否 | 预解析信号束句柄（**推荐**）；无则依赖网关/调用方约定 |
| `theme` | string | 视场景 | `theme_feed` 等需要主题维度时必填 |
| `anchor_guide_card_id` | string | 视场景 | `guide_detail_related` 等需要锚点卡片时必填 |

#### `ConsentFlags`

| 字段 | 类型 | 说明 |
|------|------|------|
| `personalization_allowed` | bool | 是否允许个性化推荐 |
| `ad_personalization_allowed` | bool | 是否允许广告个性化 |

---

### 2.2 `RecommendationFilters`

**用途**：主题、标签、语言、内容类型等 **可选过滤**；未列出的维度由策略默认处理。

| 字段 | 类型 | 说明 |
|------|------|------|
| `themes` | repeated string | 主题键列表 |
| `tags` | repeated string | 标签列表 |
| `languages` | repeated string | 语言过滤 |
| `content_types` | repeated string | 内容类型过滤 |

空 repeated 表示「不额外限制该维度」。

---

### 2.3 分页

#### 游标分页（主路径，与共享契约一致）

**请求 `CursorLimits`**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cursor` | string | 否 | 上一页响应的 `next_cursor`；首页空 |
| `limit` | int32 | 否 | 本页最大条数，**上限 100**（默认由实现或网关约定） |

**响应 `CursorPagination`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `next_cursor` | string（可选） | 下一页游标；无更多时可省略或空 |
| `has_more` | bool | 是否还有下一页 |
| `limit` | int32 | 本页请求的最大条数回显 |

#### 偏移分页（可选，短列表 / 相关条带）

**请求 `OffsetLimits`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `page` | int32 | 页码，**从 1 开始** |
| `page_size` | int32 | 每页条数，上限见共享契约 |

**响应 `OffsetPagination`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `page` | int32 | 当前页 |
| `page_size` | int32 | 页大小 |
| `total` | int32 | 总条数（若成本过高可实现为近似并在实现层注明） |
| `has_more` | bool | 是否还有下一页 |

**规则**：单次 RPC 的列表延续语义 **只选一种**：游标 **或** 偏移；两者同时填写的行为以实现为准（建议拒绝并返回 `INVALID_ARGUMENT`）。

---

### 2.4 `RecommendationStrategy`

**用途**：版本化策略句柄；**Explain** 必须携带与原结果一致的 `id` + `version`。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | string | 是 | 策略标识 |
| `version` | string | 是 | 单调版本（semver 或整型修订均可，域内统一即可） |
| `experiment_key` | string | 否 | A/B 桶键，用于分析 |
| `pipeline` | string | 否 | 命名流水线描述（召回→过滤→排序→后处理） |
| `updated_at` | string | 否 | 配置发布时间 ISO 8601 UTC |

---

### 2.5 `RecommendationItem`

**用途**：单条推荐结果 **最小引用**；富展示由 `content-domain` / 网关hydration。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `guide_card_id` | string | 是 | 稳定导购卡片 ID，与共享契约一致 |
| `content_ref` | `ContentRef` | 否 | 非卡片类内容指针 |
| `score` | double | 否 | 最终分；可省略或仅调试 |
| `rank` | int32 | 是 | **1-based** 排序位置 |
| `recall_sources` | repeated string | 否 | 粗粒度召回来源：`cf`、`content_similar`、`editorial`、`popular` 等 |
| `placement` | string | 否 | 场景内槽位 ID |
| `trace_ref` | string | 否 | 与 `tracking-domain` 曝光关联的不透明 token |
| `explanation` | `ItemExplanation` | 否 | 行内解释；未填时可由 `ExplainRecommendations` 补全 |
| `reason_tags` | repeated string | 否 | 短标签文案（与对外 `reason_tags` 对齐） |
| `experiment` | `ExperimentInfo` | 否 | 条目级实验信息 |

#### `ContentRef`

| 字段 | 类型 | 说明 |
|------|------|------|
| `kind` | string | 引用类型，如 `topic`、`article` |
| `ref_id` | string | 对应实体 ID |

#### `ExperimentInfo`

| 字段 | 类型 | 说明 |
|------|------|------|
| `key` | string | 实验或策略键 |
| `variant` | string | 分组 / 变体名 |

---

### 2.6 `ItemExplanation`（解释载荷）

**用途**：合规可用的推荐理由；**不得**编造未授权断言。

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `summary` | string | 是 | 短用户可见说明 |
| `reason_codes` | repeated string | 否 | 稳定机器可读原因码 |
| `entities` | repeated string | 否 | 可安全展示的主题 / 类目 ID |
| `confidence_band` | `ConfidenceBand` | 否 | UI 弱化用：`high` / `medium` / `low` |
| `policy_version` | string | 否 | 解释策略版本，供审计 |

#### `ConfidenceBand`（proto 枚举）

| 枚举值 | 语义 |
|--------|------|
| `CONFIDENCE_BAND_UNSPECIFIED` | 未指定 |
| `CONFIDENCE_HIGH` | 高 |
| `CONFIDENCE_MEDIUM` | 中 |
| `CONFIDENCE_LOW` | 低 |

---

### 2.7 `RequestContextEcho`

**用途**：在 **非敏感** 前提下回显请求维度，便于客户端去重与 A/B。

| 字段 | 类型 | 说明 |
|------|------|------|
| `theme` | string | 请求主题 |
| `cursor` | string | 当前页游标（若适用） |
| `query` | string | 搜索词（须脱敏与长度限制） |
| `anchor_guide_card_id` | string | 锚点卡片 ID |

---

### 2.8 `RecommendationDiagnostics`（仅 `debug = true`）

| 字段 | 类型 | 说明 |
|------|------|------|
| `trace_id` | string | 内部追踪 ID |
| `recall_lane_stats` | repeated string | 召回 lane 级诊断摘要 |

仅内部环境应开启；生产默认关闭。

---

## 3. RPC：`QueryRecommendations`

**用途**：按 `scene` 与上下文生成个性化或会话级推荐列表。

### 请求 `QueryRecommendationsRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene` | `RecommendationScene` | 是 | 见本文第 1 节 |
| `context` | `RecommendationRequestContext` | 是 | 见本文第 2.1 小节 |
| `cursor_limits` | `CursorLimits` | 否* | 游标分页，见第 2.3 小节 |
| `offset_limits` | `OffsetLimits` | 否* | 偏移分页，见第 2.3 小节 |
| `filters` | `RecommendationFilters` | 否 | 见第 2.2 小节 |
| `debug` | bool | 否 | 为 true 时可返回 `diagnostics` |

\* 至少一种分页参数语义生效：通常 feeds 使用 `cursor_limits`；若接口文档声明纯偏移面，则使用 `offset_limits`。

### 响应 `QueryRecommendationsResponse`

| 字段 | 类型 | 说明 |
|------|------|------|
| `recommendation_id` | string | **本次**推荐结果唯一 ID |
| `scene` | `RecommendationScene` | 与请求一致 |
| `generated_at` | string | 生成时间 ISO 8601 UTC |
| `strategy` | `RecommendationStrategy` | 见第 2.4 小节 |
| `items` | repeated `RecommendationItem` | 有序列表，`rank` 与 `guide_card_id` 必填 |
| `cursor_pagination` | `CursorPagination` | 使用游标时填充 |
| `offset_pagination` | `OffsetPagination` | 使用偏移时填充 |
| `request_context_echo` | `RequestContextEcho` | 见第 2.7 小节 |
| `diagnostics` | `RecommendationDiagnostics` | 仅 `debug` 时 |

### 典型错误码

见第 8 节（`INVALID_ARGUMENT`、`AUTH_REQUIRED`、`SCENE_NOT_SUPPORTED`、`SERVICE_UNAVAILABLE`）。

---

## 4. RPC：`GetPopularRecommendations`

**用途**：热门 / 趋势推荐，**弱个性化**；策略来源与 `QueryRecommendations` 不同，但 **响应形状一致**。

### 请求 `GetPopularRecommendationsRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene` | `RecommendationScene` | 是 | 推荐使用 `HOME_POPULAR` 或与产品约定的热门场景 |
| `context` | `RecommendationRequestContext` | 是 | 地区、端、语言等轻量上下文 |
| `window` | string | 否 | 统计窗口：`24h`、`7d`、`30d` 等 |
| `cursor_limits` | `CursorLimits` | 否 | 同第 2.3 小节 |
| `offset_limits` | `OffsetLimits` | 否 | 同第 2.3 小节 |
| `filters` | `RecommendationFilters` | 否 | 同第 2.2 小节 |
| `debug` | bool | 否 | 同第 3 节 |

### 响应 `GetPopularRecommendationsResponse`

与 `QueryRecommendationsResponse` **字段表完全相同**（见第 3 节响应表：`recommendation_id`、`scene`、`generated_at`、`strategy`、`items`、`cursor_pagination`、`offset_pagination`、`request_context_echo`、`diagnostics`）。

---

## 5. RPC：`ExplainRecommendations`

**用途**：为已返回条目 **补充或刷新** `ItemExplanation`；顺序与请求 `items` **一一对应**。

### 请求 `ExplainRecommendationsRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `scene` | `RecommendationScene` | 是 | 须与原推荐响应一致 |
| `context` | `RecommendationRequestContext` | 是 | 与原请求等价上下文 |
| `items` | repeated `ExplainItemRef` | 是 | 最多 N 条（N 由实现/运维配置） |
| `strategy` | `RecommendationStrategy` | 是 | 原结果中的 `id`、`version`（及可选 `experiment_key`） |

#### `ExplainItemRef`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `guide_card_id` | string | 是 | 与原始 `RecommendationItem` 一致 |
| `rank` | int32 | 是 | **1-based**，与原始 `rank` 一致 |

### 响应 `ExplainRecommendationsResponse`

| 字段 | 类型 | 说明 |
|------|------|------|
| `explanations` | repeated `ItemExplanation` | 与 `items` **同序**；第 i 条对应第 i 个 `ExplainItemRef` |

### 典型错误码

除第 8 节通用码外，条目与策略不匹配时可返回 `INVALID_ARGUMENT`（调用方宜使用同一 `recommendation_id` 关联日志，该 ID 可作为上下文日志字段而非本 RPC 必填字段）。

---

## 6. RPC：`HealthCheck`

**用途**：探活与索引 / 特征依赖检查。

### 请求 `HealthCheckRequest`

空消息。

### 响应 `HealthCheckResponse`

| 字段 | 类型 | 说明 |
|------|------|------|
| `status` | `HealthStatus` | `HEALTH_OK` / `HEALTH_DEGRADED` |
| `index_status` | `IndexDependencyStatus` | 索引或近似检索依赖 |

#### `HealthStatus`

| 枚举值 | 说明 |
|--------|------|
| `HEALTH_OK` | 正常 |
| `HEALTH_DEGRADED` | 降级可用 |

#### `IndexDependencyStatus`

| 枚举值 | 说明 |
|--------|------|
| `INDEX_UP` | 依赖可用 |
| `INDEX_DOWN` | 依赖不可用 |
| `INDEX_UNKNOWN` | 未知 / 未探测 |

---

## 7. 缓存与重试

- `QueryRecommendations`、`GetPopularRecommendations`、`ExplainRecommendations` **可安全重试**（实现应保证幂等或将副作用限于只读路径）。
- `GetPopularRecommendations` 的缓存 TTL 通常 **长于** 强个性化结果。
- 缓存键至少包含：`scene`、`strategy.version`、关键上下文维度（如 `locale`、`channel`）、分页游标或页码。

---

## 8. 错误码（`RecommendationErrorCode`）

业务层与 gRPC `details` 应对齐下列 **整数码**（与 proto 枚举取值一致）：

| 码 | 枚举名 | 说明 |
|----|--------|------|
| `10002` | `INVALID_ARGUMENT` | 参数校验失败（含场景必填字段缺失、分页混用等） |
| `20001` | `AUTH_REQUIRED` | 需要登录但当前为访客态（若该 scene 要求登录） |
| `40001` | `SCENE_NOT_SUPPORTED` | 场景不支持或未知 |
| `40002` | `SERVICE_UNAVAILABLE` | 推荐依赖不可用 / 熔断 |

`0` / `RECOMMENDATION_ERROR_UNSPECIFIED` 不使用作成功或明确失败语义。

---

## 9. 版本管理

- 对外 API 版本由 `gateway` 控制；**本域**以目录 `proto` 与包 `simple_living.recommendation_domain` 为边界。
- 破坏性变更须同步：本文、`docs/contracts/` 中相关字段、`gateway` 映射与 `changelog.md`（若仓库要求）。

---

## 10. `RecommendationService` 方法一览

| RPC | 请求 | 响应 |
|-----|------|------|
| `QueryRecommendations` | `QueryRecommendationsRequest` | `QueryRecommendationsResponse` |
| `GetPopularRecommendations` | `GetPopularRecommendationsRequest` | `GetPopularRecommendationsResponse` |
| `ExplainRecommendations` | `ExplainRecommendationsRequest` | `ExplainRecommendationsResponse` |
| `HealthCheck` | `HealthCheckRequest` | `HealthCheckResponse` |
