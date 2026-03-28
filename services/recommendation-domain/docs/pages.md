# recommendation-domain — Page and surface mapping (v1)

Maps **product surfaces** to **scenes**, expected **internal recommendation APIs**, and **ownership** for parallel work. Clients consume public JSON from `gateway`; this file describes the recommendation-domain side of the mapping. Full card payloads are loaded from `content-domain` unless BFF denormalization is explicitly agreed.

## 1. Homepage

| UI block | Scene | API | Notes |
|----------|--------|-----|--------|
| Primary feed | `home_feed` | `QueryRecommendations` | Personalized when consent allows; fallback to session or popular blend |
| Popular / trending row | `home_popular` | `GetPopularRecommendations` | Cached more aggressively; optional region facet in `context` |
| “For you” modules | `home_feed` + `placement` | `QueryRecommendations` | 通过 `placement` 或模块键区分子模块，避免场景键膨胀 |

**Gateway**: May aggregate multiple calls; each block carries its own `placement` for analytics.

---

## 2. Feed (standalone feed screen)

| UI block | Scene | API | Notes |
|----------|--------|-----|--------|
| Infinite list | `home_feed` | `QueryRecommendations` | Cursor pagination；与首页主流保持同一场景语义，必要差异通过 `placement` 表达 |
| Pull-to-refresh | `home_feed` | `QueryRecommendations` | New `recommendation_id`; optional “exclude recent ids” in `filters` if contract added |

---

## 3. Category (theme / channel browse)

| UI block | Scene | API | Notes |
|----------|--------|-----|--------|
| Category ranked list | `theme_feed` | `QueryRecommendations` | Use `filters.theme_id` from `content-domain` taxonomy |
| Category hot picks | `home_popular` + `filters.theme_id` | `GetPopularRecommendations` | 热门能力复用，不单独新增场景语义 |
| Subcategory tabs | `theme_feed` + `filters.theme_id` | `QueryRecommendations` | 子主题通过过滤条件表达，不单独膨胀场景键 |

**Dependency**: Category ids and tree come from `content-domain`; recommendation-domain does not define taxonomy.

---

## 4. Detail (article, list, card detail)

| UI block | Scene | API | Notes |
|----------|--------|-----|--------|
| Related content | `guide_detail_related` | `QueryRecommendations` | Pass current `guide_card_id` or `content_ref` in `context` |
| “Readers also viewed” | `guide_detail_related` + `placement=also_viewed` | `QueryRecommendations` | 复用场景，差异通过 placement 或 filter 表达 |
| Author more | `guide_detail_related` + `filters.author_id` | `QueryRecommendations` | 强 author filter；不额外新增场景键 |

**Context**: Caller must pass **current content** identifier so recall can use content similarity without storing raw body here.

---

## 5. Cross-cutting UI behaviors

| Behavior | Handling |
|----------|----------|
| Empty state | 默认返回成功结果 + `items: []`；是否降级到 content-domain 兜底由 gateway 决定 |
| Lazy explanations | After render, call `ExplainRecommendations` for visible cells |
| Logged-out mode | `QueryRecommendations` with `session_id` only; `personalization_mode` per scene |

---

## 6. Non-pages (API-only)

Mini-program or third-party widgets may call the same scenes with different `channel` values; **scene** remains the primary routing key for strategy.
