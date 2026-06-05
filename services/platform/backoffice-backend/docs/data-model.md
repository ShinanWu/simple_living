# backoffice-backend — 数据模型

权威存储：**单个 PostgreSQL 数据库**，三 schema 隔离模块。迁移文件位于 `services/foundation/migrations/`，命名 `NNNN_<module>_<change>.sql`。

## 1. Schema 划分

| Schema | 来源 | 说明 |
|--------|------|------|
| `content` | 原 platform/backoffice-backend | 导购实体、主题、版本 |
| `governance` | 原 platform/backoffice-backend | 审核、可见性、披露 |
| `affiliate` | 原 platform/backoffice-backend | partner、佣金规则 |
| `platform` | 新增 | outbox、导出作业元数据 |

完整列级 DDL 自原三域 `data-model.md` 迁入（实现前对照合并）；下列为结构与新增表要点。

## 2. platform 元数据（新增）

```sql
CREATE TABLE platform.schema_migrations (
  version     TEXT PRIMARY KEY,
  applied_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

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
  bundle_type     TEXT NOT NULL,  -- catalog_snapshot | visibility_index | affiliate_link_spec
  version         BIGINT NOT NULL,
  status          TEXT NOT NULL,  -- pending | staging | active | failed
  file_path       TEXT,
  sha256          TEXT,
  created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
  activated_at    TIMESTAMPTZ
);
CREATE UNIQUE INDEX uq_export_active_bundle ON platform.export_job (bundle_type, version);
```

## 3. 导出物逻辑模型（非 SQL）

### catalog_snapshot

- 已发布且 governance 可见的 `guide_card`、`topic`、`ranking` 只读投影
- 含 `theme`、`tags`、`disclosure_labels`（来自 governance 叠加）
- 二进制格式 v1：长度前缀 + FlatBuffers（或 protobuf frozen schema `export_catalog.proto`）

### visibility_index

- `content_id → visibility_enum` 稠密索引 / roaring bitmap
- 下架与 `restricted` 必须可在 snapshot 应用前独立 patch

### affiliate_link_spec

- per `partner_id`：`channel_code`、URL 模板、参数槽位、能力 flags、`secret_version`
- **禁止**包含 `signing_key` 明文

## 4. 迁移编号（canonical）

| 版本 | 文件 | 内容 |
|------|------|------|
| 0001 | `0001_content_init.sql` | content schema |
| 0003 | `0003_affiliate_init.sql` | affiliate schema |
| 0006 | `0006_governance_init.sql` | governance schema |
| 0010 | `0010_platform_export_init.sql` | platform outbox + export_job |

> 编号与原单域文档一致，合并后仍用分文件迁移，便于回滚。

## 5. 保留与隐私

- 运营 actor、`request_id` 写入各模块审计列
- affiliate 密钥不落库明文；`secret_version` 可审计
- 软删除内容保留 180 天归档后物理清理（可配置）
