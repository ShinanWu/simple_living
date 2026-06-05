# backoffice-backend — 工作流

## 1. 运营写主链路（进程内）

### 1.1 内容创作 → 审核 → 发布

```text
backoffice-web → gateway(/api/v2/backoffice/content/*)
  → backoffice-backend.content 模块（草稿/Upsert）
  → backoffice-backend.governance 模块（SubmitReview / 审核态）
  → governance 可见性裁决（Approve 不自动 Publish）
  → backoffice-backend.content 模块（Publish，服从可见性）
  → 事务提交 + outbox 事件
  → 导出 job：重建 catalog_snapshot + visibility_index
  → recommendation-server mmap 热切换
```

幂等：写接口携带 `idempotency_key`；乐观锁冲突返回 `10007`。

### 1.2 紧急下架 / 可见性限制

```text
gateway(/api/v2/backoffice/governance/visibility)
  → governance 模块写入 visibility 裁决
  → 优先导出通道（priority lane）patch visibility_index
  → recommendation-server 热切换（目标 ≤ 1s）
```

读路径 fail-closed：snapshot 中不可见的内容不得被 `recommendation-server` 返回。

### 1.3 联盟伙伴配置

```text
gateway(/api/v2/backoffice/affiliate/*)
  → affiliate 模块 UpsertPartner / CommissionRule
  → 导出 affiliate_link_spec（脱敏，无 signing secret）
  → tracking-server 热加载
```

密钥轮换：仅 affiliate 模块从 secret backend 拉取；导出物只含 `secret_version` 与模板槽位，tracking 侧通过独立 secret 注入完成签名。

## 2. 导出发布（数据面）

### 2.1 导出 bundle

| Bundle | 文件名模式 | 触发 |
|--------|------------|------|
| `catalog_snapshot` | `catalog_{version}.bin` + `catalog.manifest` | content 发布、专题/榜单变更 |
| `visibility_index` | `visibility_{version}.bin` | 审核/可见性/下架 |
| `affiliate_link_spec` | `affiliate_spec_{version}.bin` | partner/规则变更 |

`manifest` 字段：`version`（单调递增 uint64）、`sha256`、`generated_at`、`bundle_type`、`min_reader_version`。

### 2.2 发布步骤

1. PostgreSQL 事务提交业务变更。
2. 同事务或 follow-up 写入 `platform_outbox`（topic 见 data-model.md）。
3. 导出 worker 生成 staging 文件到 `{export_dir}/staging/`。
4. 校验 sha256 与 schema 版本后原子 rename 至 `{export_dir}/active/`。
5. 更新 `active/*.manifest` 的 `active_version` 指针（原子 replace）。
6. 读服务 inotify / 定时轮询 manifest，mmap staging → 校验 → 切换 active 指针。

### 2.3 一致性与降级

| 场景 | 行为 |
|------|------|
| 导出 job 失败 | 保持上一版 active；写 RPC 仍成功；告警 `export_lag_seconds` |
| 读服务启动无 manifest | 进程拒绝就绪（readiness 失败），不 serve 空 catalog |
| 下架优先通道 | 可仅 patch `visibility_index` 而不等待全量 catalog 重建 |

## 3. 领域事件（Kafka，at-least-once）

| Topic | Key | 消费方 |
|-------|-----|--------|
| `platform.content.published` | `content_id` | 分析、候选池预热（可选） |
| `platform.governance.visibility_changed` | `content_id` | 审计、告警 |
| `platform.affiliate.spec_changed` | `partner_id` | 运营通知 |

事件 payload 版本与字段见 [data-model.md](./data-model.md)；读服务**不**依赖 Kafka 做在线读，仅作辅助与审计。

## 4. 与 gateway backoffice 路由对应

完整 HTTP 路由与权限见 `services/gateway/docs/backoffice-backend.md` 与 `api.md` §Backoffice 映射表。gateway 对 backoffice 写路径**单服务转发**，不做跨模块分布式事务；多模块编排由本服务进程内顺序完成。
