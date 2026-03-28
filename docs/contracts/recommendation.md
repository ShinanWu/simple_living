# 推荐结果契约（Recommendation）

## 1. 目的

定义**推荐接口返回的共享结果结构**：场景（scene）、条目列表与可选解释字段；列表项引用 [guide-card.md](./guide-card.md) 中的实体，可内联完整卡片或仅返回 ID 由客户端批量拉取（由具体接口约定）。

## 2. 顶层结果对象

推荐接口成功时，`data` 中通常包含：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `recommendation_id` | string | 是 | 本次推荐响应的唯一 ID，用于曝光/点击与调试串联 |
| `scene` | string | 是 | 推荐场景标识，见第 3 节 |
| `generated_at` | string | 是 | ISO 8601 UTC，推荐结果生成时间 |
| `items` | array | 是 | 有序推荐条目，见第 4 节 |
| `request_context_echo` | object | 否 | 安全范围内回显请求中的上下文字段，便于客户端去重与 A/B |
| `strategy` | object | 否 | 可选；推荐策略信息，如 `id`、`version`、`experiment_key` |

## 3. 场景 `scene` 约定

`scene` 为稳定枚举字符串（`snake_case`），新增场景仅**追加**：

| scene | 说明 |
|-------|------|
| `home_feed` | 首页信息流 |
| `home_popular` | 首页热门或趋势模块 |
| `theme_feed` | 某主题（衣/食/住/行）列表或频道 |
| `guide_detail_related` | 导购详情页相关推荐 |
| `search` | 搜索结果的推荐穿插（若与搜索同包可仍用此值并靠 `request_context_echo` 区分） |
| `cold_start` | 冷启动/新用户默认流 |

具体接口必须在文档中声明允许的 `scene` 子集及必填请求参数。

## 4. 推荐条目 `items[]`

每项：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `rank` | integer | 是 | 从 **1** 开始的排序位置 |
| `guide_card_id` | string | 是 | 对应导购卡片 ID |
| `guide_card` | object | 否 | 若接口约定内联，则符合 [guide-card.md](./guide-card.md)；否则省略由客户端再请求 |
| `score` | number | 否 | 可选排序分，仅供调试或灰度；**禁止**作为合同保证的排序依据展示给用户 |
| `reason_tags` | array of string | 否 | 短解释标签，如 `["相似用户也喜欢"]` |
| `experiment` | object | 否 | 实验桶信息，见 4.1 |

### 4.1 `experiment`（可选）

| 子字段 | 类型 | 说明 |
|--------|------|------|
| `key` | string | 实验或策略键 |
| `variant` | string | 分组或变体名 |

## 5. `request_context_echo` 建议字段（均可选）

仅回显**非敏感**信息，例如：

| 字段 | 类型 | 说明 |
|------|------|------|
| `theme` | string | 请求的主题，同 `guide-card.theme` 枚举 |
| `cursor` | string | 若列表为游标分页，可回显当前游标 |
| `query` | string | 搜索词（注意脱敏与长度限制） |

## 6. 分页

若推荐列表支持分页，须符合 [pagination.md](./pagination.md)；`items` 为当前页条目，`pagination` 与推荐顶层字段并列于同一 `data` 对象内。

## 7. 示例（内联卡片 + 游标分页）

```json
{
  "recommendation_id": "rec_01jqxyz",
  "scene": "home_feed",
  "generated_at": "2026-03-28T10:00:00Z",
  "items": [
    {
      "rank": 1,
      "guide_card_id": "550e8400-e29b-41d4-a716-446655440000",
      "reason_tags": ["编辑精选"],
      "guide_card": {
        "guide_card_id": "550e8400-e29b-41d4-a716-446655440000",
        "schema_version": 1,
        "title": "春季通勤外套怎么选",
        "theme": "clothing",
        "published_at": "2026-03-28T08:00:00Z",
        "updated_at": "2026-03-28T09:30:00Z",
        "commercial_disclosure": { "is_commercial": true }
      }
    }
  ],
  "pagination": {
    "next_cursor": "c_next",
    "has_more": false,
    "limit": 20
  },
  "request_context_echo": {
    "theme": "clothing"
  }
}
```

## 8. 兼容性

仅允许新增可选字段；`scene` 新值不得占用已有语义；`rank` 语义保持不变。
