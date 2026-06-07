# 后端数据模型

`backoffice-backend` 权威存储：单个 PostgreSQL，按 schema 隔离。迁移位于 `services/foundation/migrations/`。

产品语义见 [product-spec.md](./product-spec.md)；页面与 HTTP 映射见 [detail-design.md](./detail-design.md)。

## Schema 划分

| Schema | 职责 |
|--------|------|
| `content` | 导购实体、主题、版本 |
| `governance` | 审核队列、可见性裁决、披露 |
| `affiliate` | partner、佣金规则 |
| `platform` | outbox、export_job 元数据 |

## platform 元数据

```sql
CREATE TABLE platform.outbox (
  id            BIGSERIAL PRIMARY KEY,
  topic         TEXT NOT NULL,
  message_key   TEXT NOT NULL,
  payload_json  JSONB NOT NULL,
  created_at    TIMESTAMPTZ NOT NULL DEFAULT now(),
  published_at  TIMESTAMPTZ
);
CREATE INDEX idx_platform_outbox_unpublished ON platform.outbox (created_at) WHERE published_at IS NULL;

CREATE TABLE platform.export_job (
  id              BIGSERIAL PRIMARY KEY,
  bundle_type     TEXT NOT NULL,
  version         BIGINT NOT NULL,
  status          TEXT NOT NULL,
  file_path       TEXT,
  sha256          TEXT,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  activated_at    TIMESTAMPTZ
);
CREATE UNIQUE INDEX uq_export_active_bundle ON platform.export_job (bundle_type, version);
```

## 导出物（读侧消费）

| Bundle | 消费方 | 内容 |
|--------|--------|------|
| `catalog_snapshot` | recommendation-server | 已发布且可见的 guide_card / topic / ranking 投影 |
| `visibility_index` | recommendation-server | content_id → 可见性枚举 |
| `affiliate_link_spec` | tracking-server | 转链规格（**不含** signing secret） |

manifest 字段：`version`、`sha256`、`generated_at`、`bundle_type`、`min_reader_version`。

## 迁移编号

| 版本 | 文件 |
|------|------|
| 0001 | `0001_content_init.sql` |
| 0003 | `0003_affiliate_init.sql` |
| 0006 | `0006_governance_init.sql` |
| 0010 | `0010_platform_export_init.sql` |

部署前执行 `services/foundation/scripts/run_migrations.sh`。

## 保留与隐私

- 运营 actor、`request_id` 写入各模块审计列
- affiliate 密钥不落库明文；导出物仅含 `secret_version`
- 软删除内容保留 180 天后物理清理（可配置）
