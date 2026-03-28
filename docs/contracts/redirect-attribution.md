# 跳转与归因字段（Redirect & Attribution）

## 1. 目的

约定从**点击导购内容**到**第三方成交**链路上的标识字段与语义，供 tracking-domain、affiliate-domain 与各端埋点共用；不包含具体联盟平台 API 细节。

## 2. 核心标识

| 字段 | 类型 | 说明 |
|------|------|------|
| `click_id` | string | 单次用户点击的唯一 ID，由服务端在记录点击时生成并返回给客户端 |
| `trace_id` | string | 可选；跨服务追踪 ID，可与 [common-response.md](./common-response.md) `meta.trace_id` 对齐 |
| `session_id` | string | 可选；访客或登录会话 ID，与 auth 域内部会话一致即可 |
| `user_id` | string | 可选；登录用户 ID；访客勿强行填假值 |

## 3. 渠道与载体

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel_code` | string | 联盟或跳转渠道编码，如 `pdd`、`douyin`；与 [guide-card.md](./guide-card.md) 中 `affiliate_context.channel_codes` 取值域一致 |
| `client_platform` | string | 与 [auth.md](./auth.md) `X-Client-Platform` 同枚举 |
| `client_version` | string | 可选 |

## 4. 内容与推荐来源

用于归因到内容与推荐策略：

| 字段 | 类型 | 说明 |
|------|------|------|
| `guide_card_id` | string | 被点击的导购卡片 |
| `recommendation_id` | string | 可选；若点击发生在推荐列表内，填当时接口返回的 `recommendation_id` |
| `scene` | string | 可选；推荐场景，同 [recommendation.md](./recommendation.md) |
| `item_rank` | integer | 可选；推荐列表中的 `rank` |

## 5. 跳转请求/响应中的常见字段

### 5.1 客户端发起「解析跳转」（示例语义）

本节只定义共享字段语义，不定义真实对外路径、HTTP 方法或内部 RPC 名称；具体路由由 `gateway` 文档维护。

请求可携带：

| 字段 | 说明 |
|------|------|
| `guide_card_id` | 必填 |
| `click_id` | 若已预分配则携带；否则由服务端创建后返回 |
| `preferred_channel_code` | 可选；用户或客户端偏好渠道 |

响应 `data` 建议：

| 字段 | 类型 | 说明 |
|------|------|------|
| `landing_url` | string | HTTPS 落地 URL，客户端 WebView 或外链打开 |
| `click_id` | string | 最终生效的点击 ID |
| `expires_at` | string | 可选；ISO 8601，链接过期时间 |
| `attribution` | object | 可选；回显供埋点用的只读字段子集 |

### 5.2 `landing_url` 约定

- 必须为 **HTTPS**（特殊小程序 web-view 规则以平台为准）。
- 客户端**不得**在未记录点击的情况下自行拼接联盟参数替换服务端下发 URL。

## 6. 转化回传（逻辑字段）

联盟或第三方回调进入平台时，建议能关联到以下键（名称可按网关实际映射，语义须一致）：

| 字段 | 说明 |
|------|------|
| `click_id` | 与点击记录关联 |
| `external_order_id` | 第三方订单或转化单号（若可得） |
| `conversion_type` | 如 `pay`, `register`, `lead` |
| `converted_at` | ISO 8601 |
| `commission_amount` | 可选；结构化金额对象见下 |
| `currency` | 可选；ISO 4217，如 `CNY` |

金额对象（若使用）：

| 字段 | 类型 | 说明 |
|------|------|------|
| `amount_minor` | integer | 最小货币单位，如分 |
| `currency` | string | 与顶层 `currency` 一致或可省略一处 |

## 7. 隐私与最小必要

- 回传与存储须遵守各端隐私政策；`device_id` 等仅在授权与合规前提下使用。
- 对外部回调体字段做签名校验与幂等，属实现要求，不在此扩展。

## 8. 示例：点击记录 + 跳转响应

**记录点击（简化）**

```json
{
  "guide_card_id": "550e8400-e29b-41d4-a716-446655440000",
  "recommendation_id": "rec_01jqxyz",
  "scene": "home_feed",
  "item_rank": 1,
  "channel_code": "pdd",
  "client_platform": "ios"
}
```

**响应**

```json
{
  "click_id": "clk_01jqabc",
  "landing_url": "https://example.com/r?sig=...",
  "expires_at": "2026-03-28T11:00:00Z"
}
```

## 9. 兼容性

仅允许新增可选字段；`click_id` 生成规则变更须保证旧数据可查询或迁移说明。
