# 导购卡片契约（Guide Card）

## 1. 目的

定义平台内**可被推荐、列表、详情页消费的统一内容卡片**结构（纯导购、无站内交易）；与 [recommendation.md](./recommendation.md) 中的条目引用同一实体。

## 2. 标识与版本

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `guide_card_id` | string | 是 | 全局唯一 ID，建议 UUID |
| `schema_version` | integer | 是 | 卡片 JSON 结构版本，从 `1` 递增；客户端据此做兼容解析 |

## 3. 展示字段

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `title` | string | 是 | 主标题 |
| `subtitle` | string | 否 | 副标题 |
| `summary` | string | 否 | 短摘要/卖点聚合，纯文本或轻量标记由内容规范另定 |
| `cover_media` | object | 否 | 见 3.1 |
| `theme` | string | 是 | 业务主题：`clothing` \| `food` \| `housing` \| `mobility`（与产品「衣食住行」对齐） |
| `tags` | array of string | 否 | 展示用标签，如 `["性价比", "通勤"]` |
| `published_at` | string | 是 | ISO 8601 UTC，如 `2026-03-28T08:00:00Z` |
| `updated_at` | string | 是 | ISO 8601 UTC |

### 3.1 `cover_media`

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | string | `image` \| `video_poster`（扩展须加版本） |
| `url` | string | HTTPS 可访问地址 |
| `width` | integer | 可选，像素 |
| `height` | integer | 可选，像素 |

## 4. 商业披露（合规）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `commercial_disclosure` | object | 是 | 见下 |

| 子字段 | 类型 | 说明 |
|--------|------|------|
| `is_commercial` | boolean | 是否含商业合作/联盟推广 |
| `partner_id` | string | 否 | 统一合作伙伴或计划标识，用于展示「广告」类文案时的数据源 |
| `disclosure_text_key` | string | 否 | 客户端文案资源 key，避免服务端硬编码长文案 |

## 5. 跳转与联盟负载（与 tracking 衔接）

卡片**不直接下单**，须通过跳转服务生成落地链。此处仅约定**最小可传递负载**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `affiliate_context` | object | 否 | 供 affiliate/tracking 域生成链接的上下文，结构由 [redirect-attribution.md](./redirect-attribution.md) 引用方扩展；客户端**不得**篡改后回传 |

建议最小键（若存在）：

| 子字段 | 类型 | 说明 |
|--------|------|------|
| `offer_id` | string | 平台内选品/offer 标识 |
| `channel_codes` | array of string | 可用渠道编码列表（排序策略服务端决定） |

具体跳转 URL **不由**卡片静态字段承载，由「获取跳转/转链」类接口返回，以免链接过期与签名校验泄露。

## 6. 扩展桶

| 字段 | 类型 | 说明 |
|------|------|------|
| `attributes` | object | 可选 KV，键为 `snake_case`，值仅 JSON 安全类型；用于垂直场景扩展 |

## 7. 完整示例

```json
{
  "guide_card_id": "550e8400-e29b-41d4-a716-446655440000",
  "schema_version": 1,
  "title": "春季通勤外套怎么选",
  "subtitle": "三件够用一周",
  "summary": "兼顾防风与透气，按预算分档。",
  "cover_media": {
    "type": "image",
    "url": "https://cdn.example.com/covers/abc.jpg",
    "width": 1200,
    "height": 800
  },
  "theme": "clothing",
  "tags": ["通勤", "性价比"],
  "published_at": "2026-03-28T08:00:00Z",
  "updated_at": "2026-03-28T09:30:00Z",
  "commercial_disclosure": {
    "is_commercial": true,
    "partner_id": "pdd_jinbao_001",
    "disclosure_text_key": "disclosure.affiliate.default"
  },
  "affiliate_context": {
    "offer_id": "offer_8821",
    "channel_codes": ["pdd", "douyin"]
  },
  "attributes": {
    "reading_time_minutes": 3
  }
}
```

## 8. 兼容性

- 仅允许**新增**可选字段或 `attributes` 键；禁止移除已发布必填字段。
- 客户端须忽略未知顶层键；`schema_version` 升级时由发布说明约定破坏性变更节奏。
