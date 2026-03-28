# 分页约定（Pagination）

## 1. 目的

统一列表类接口的**请求参数**与**响应结构**，避免 Web / App / 小程序各自定义一套分页字段。

## 2. 两种模式

| 模式 | 适用场景 | 请求主键 | 响应延续键 |
|------|----------|----------|------------|
| **偏移分页（offset）** | 总条数明确、页码翻页、管理端 | `page`, `page_size` | `total`, `has_more` |
| **游标分页（cursor）** | 信息流、高实时、大列表 | `cursor`, `limit` | `next_cursor`, `has_more` |

同一接口**只选一种**模式并在接口文档中固定；若历史原因并存，须在路径或版本上区分。

## 3. 请求参数

### 3.1 偏移分页

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `page` | integer | 否 | 页码，**从 1 开始**；缺省建议 `1` |
| `page_size` | integer | 否 | 每页条数；**上限 100**（若业务需要更小上限须在接口中写明） |

### 3.2 游标分页

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `cursor` | string | 否 | 上一页响应中的 `next_cursor`；首页不传或传空 |
| `limit` | integer | 否 | 本页最大条数；**上限 100** |

**命名约束**：禁止使用 `offset` + `limit` 与 `page` + `page_size` 在同一接口混用且无文档说明。

## 4. 响应结构（位于 `data` 内）

列表接口在 [common-response.md](./common-response.md) 的 `data` 中推荐使用以下**统一键名**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `items` | array | 当前页实体列表 |
| `pagination` | object | 分页元信息，见下 |

### 4.1 `pagination`（偏移）

```json
{
  "page": 2,
  "page_size": 20,
  "total": 153,
  "has_more": true
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `page` | integer | 当前页码 |
| `page_size` | integer | 本页请求的大小 |
| `total` | integer | 符合条件的总条数（若计算成本过高可约定为可选或近似，须在接口说明） |
| `has_more` | boolean | 是否还有下一页 |

### 4.2 `pagination`（游标）

```json
{
  "next_cursor": "eyJwIjoxLCJ0IjoxNzExNjAzMjAwfQ",
  "has_more": true,
  "limit": 20
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `next_cursor` | string \| null | 下一页游标；无更多时为 `null` |
| `has_more` | boolean | 是否还有下一页 |
| `limit` | integer | 本页请求的最大条数 |

游标为**不透明字符串**，客户端不得解析其内容。

## 5. 完整 `data` 示例（游标 + 导购卡片 ID）

```json
{
  "items": [
    { "guide_card_id": "550e8400-e29b-41d4-a716-446655440000" },
    { "guide_card_id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8" }
  ],
  "pagination": {
    "next_cursor": "c_def456",
    "has_more": true,
    "limit": 20
  }
}
```

## 6. 空列表

无数据时：`items` 为空数组，`has_more` 为 `false`；偏移模式下 `total` 可为 `0`。**不推荐**用 HTTP 404 表示空列表（除非接口语义为「该资源下的子列表资源不存在」）。

## 7. 兼容性

仅允许为 `pagination` 或 `items` 元素**增加可选字段**；不得更名已有键或改变 `page` 起始下标约定。
