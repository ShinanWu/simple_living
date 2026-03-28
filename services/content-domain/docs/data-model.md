# content-domain 数据模型

> 实体名为逻辑模型，表结构实现时可拆分或归一化；对外 ID 建议统一为字符串 `*_id`（与 `docs/contracts/` 命名规范一致）。

## 1. 枚举

### 1.1 `LifeTheme`（衣 / 食 / 住 / 行）

| 值 | 说明 |
|----|------|
| `CLOTHING` | 衣 |
| `FOOD` | 食 |
| `HOUSING` | 住 |
| `MOBILITY` | 行 |

子主题通过 `Theme` 实体的树形或 `parent_id` 表达，不重复造四套服务。

### 1.2 `ContentLifecycleStatus`

内容在**内容域 + 治理联动**下的生命周期（与 governance 审核状态可拆分或映射）。

| 值 | 说明 |
|----|------|
| `DRAFT` | 草稿，不可对公网读者可见 |
| `IN_REVIEW` | 已提交审核 |
| `PUBLISHED` | 已发布且治理允许展示 |
| `SCHEDULED` | 已批准，未到生效时间 |
| `OFFLINE` | 运营或治理下架 |
| `ARCHIVED` | 归档，仅内部查询 |

### 1.3 `GuideCardType`

| 值 | 说明 |
|----|------|
| `PHYSICAL_GOOD` | 实物商品导购 |
| `DIGITAL_GOOD` | 虚拟/数字商品 |
| `LOCAL_SERVICE` | 本地生活服务 |
| `OTHER` | 其他（扩展保留） |

### 1.4 `MediaType`

| 值 | 说明 |
|----|------|
| `IMAGE` | 图片 |
| `VIDEO` | 视频 |
| `ICON` | 小图标 |

### 1.5 `EditorialBlockType`

| 值 | 说明 |
|----|------|
| `PARAGRAPH` | 富文本段落 |
| `HEADING` | 标题 |
| `IMAGE` | 配图 |
| `EMBED_CARD` | 嵌入导购卡片 |
| `QUOTE` | 引用 |
| `BULLET_LIST` | 列表 |

### 1.6 `TopicModuleType`

| 值 | 说明 |
|----|------|
| `CARD_GRID` | 卡片网格 |
| `CARD_CAROUSEL` | 横滑卡片 |
| `EDITORIAL_LIST` | 图文列表 |
| `RANKING_EMBED` | 嵌入整块榜单 |
| `CUSTOM_BANNER` | 头图/运营横幅位 |

### 1.7 `RankingUpdateCadence`（展示用）

| 值 | 说明 |
|----|------|
| `REALTIME` | 文案宣称实时（实际仍由运营/规则更新） |
| `DAILY` | 日更 |
| `WEEKLY` | 周更 |
| `MANUAL` | 人工不定期 |

---

## 2. 核心实体

### 2.1 `Theme`（主题）

| 字段 | 类型 | 说明 |
|------|------|------|
| `theme_id` | string | 主键 |
| `parent_id` | string, optional | 父主题 |
| `life_theme` | `LifeTheme` | 所属衣食住行大类 |
| `slug` | string | URL / 埋点用短键 |
| `display_name` | string | 展示名 |
| `description` | string, optional | 描述 |
| `icon_url` | string, optional | 图标 |
| `sort_order` | int | 同级排序 |
| `status` | enum | `ACTIVE` / `INACTIVE` |

### 2.2 `Tag`

| 字段 | 类型 | 说明 |
|------|------|------|
| `tag_id` | string | 主键 |
| `name` | string | 标签名 |
| `theme_ids[]` | string | 可选，标签与主题关联 |

### 2.3 `GuideCard`（导购卡片）

| 字段 | 类型 | 说明 |
|------|------|------|
| `card_id` | string | 主键 |
| `type` | `GuideCardType` | 卡片类型 |
| `title` / `subtitle` | string | 标题与副标题 |
| `cover_media` | `MediaRef` | 封面 |
| `media_gallery[]` | `MediaRef` | 可选，多图多视频 |
| `selling_points[]` | string | 卖点短语 |
| `price_hint` | string, optional | 展示价或价带文案，非结算价 |
| `theme_ids[]` | string | 归属主题 |
| `tag_ids[]` | string | 标签 |
| `affiliate_refs` | `AffiliateRef[]` | 渠道侧标识，结构体见下 |
| `commercial_disclosure_required` | bool | 是否需展示合作推广类说明 |
| `content_status` | `ContentLifecycleStatus` | 生命周期状态 |
| `published_revision` | int | 当前对外版本号 |
| `effective_from` / `effective_to` | timestamp, optional | 生效区间 |

**`MediaRef`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `media_id` | string | 可选，素材库 ID |
| `url` | string | CDN 地址 |
| `type` | `MediaType` | 类型 |
| `width` / `height` | int, optional | 展示比例辅助 |

**`AffiliateRef`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `channel` | string | 如 `PDD` / `DOUYIN`，与 `affiliate-domain` 字典对齐 |
| `external_item_id` | string | 渠道侧商品或套餐 ID（opaque） |
| `external_shop_id` | string, optional | 店铺 ID |
| `payload` | map, optional | 扩展字段，**本域不解析转链规则** |

### 2.4 `EditorialContent`（攻略 / 图文）

| 字段 | 类型 | 说明 |
|------|------|------|
| `content_id` | string | 主键 |
| `title` | string | 标题 |
| `summary` | string, optional | 列表摘要 |
| `cover` | `MediaRef`, optional | 列表封面 |
| `author_display_name` | string, optional | 展示用作者名 |
| `theme_ids[]` / `tag_ids[]` | string | 分类 |
| `blocks[]` | `EditorialBlock` | 正文块 |
| `content_status` | `ContentLifecycleStatus` | 状态 |
| `published_at` | timestamp, optional | 发布时间 |

**`EditorialBlock`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `block_id` | string | 块 ID |
| `type` | `EditorialBlockType` | 类型 |
| `text` | string, optional | 文本类 |
| `media` | `MediaRef`, optional | 媒体类 |
| `card_id` | string, optional | `EMBED_CARD` 时 |

### 2.5 `Topic`（专题）

| 字段 | 类型 | 说明 |
|------|------|------|
| `topic_id` | string | 主键 |
| `title` / `subtitle` | string | 标题 |
| `banner` | `MediaRef`, optional | 头图 |
| `theme_ids[]` | string | 归属主题 |
| `modules[]` | `TopicModule` | 模块列表 |
| `content_status` | `ContentLifecycleStatus` | 状态 |
| `seo_slug` | string, optional | 可选 SEO |

**`TopicModule`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `module_id` | string | 模块 ID |
| `type` | `TopicModuleType` | 模块类型 |
| `title` | string, optional | 模块标题 |
| `sort_order` | int | 模块顺序 |
| `items[]` | `TopicModuleItem` | 条目 |

**`TopicModuleItem`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `item_id` | string | 条目行 ID |
| `card_id` | string, optional | 引用卡片 |
| `content_id` | string, optional | 引用图文 |
| `ranking_id` | string, optional | 引用榜单 |
| `caption` | string, optional | 短说明 |
| `sort_order` | int | 模块内顺序 |

### 2.6 `RankingList`（榜单）

| 字段 | 类型 | 说明 |
|------|------|------|
| `ranking_id` | string | 主键 |
| `title` / `subtitle` | string | 标题 |
| `cover` | `MediaRef`, optional | 入口图 |
| `theme_ids[]` | string | 归属主题 |
| `rule_summary` | string | 榜单规则一句话 |
| `update_cadence` | `RankingUpdateCadence` | 更新节奏展示 |
| `entries[]` | `RankingEntry` | 有序条目 |
| `content_status` | `ContentLifecycleStatus` | 状态 |

**`RankingEntry`**

| 字段 | 类型 | 说明 |
|------|------|------|
| `entry_id` | string | 行 ID |
| `rank` | int | 名次，从 1 开始 |
| `card_id` / `content_id` | string, optional | 二选一或按产品约定 |
| `blurb` | string, optional | 榜单短评 |

---

## 3. 版本与审计（建议字段）

运营写入型实体（`GuideCard`、`EditorialContent`、`Topic`、`RankingList`）建议统一：

| 字段 | 说明 |
|------|------|
| `created_at` / `updated_at` | 时间戳 |
| `created_by` / `updated_by` | 运营账号 ID |
| `revision` | 乐观锁版本 |

完整审计日志可由 `governance-domain` 或基础设施承接，本域至少保留**当前对外版本**与**草稿版本**的区分策略（实现期定稿）。

---

## 4. 与契约目录的关系

- 对外 JSON/brpc 字段命名应与 `docs/contracts/guide-card.md`（待成文时以架构 README 清单为准）对齐；若契约与本文件冲突，**以契约为准**并更新本文件。
