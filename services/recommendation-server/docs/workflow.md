# recommendation-server — 工作流

## 1. Snapshot 热加载

```text
启动 → 读取 {snapshot_dir}/active/*.manifest
     → mmap catalog_snapshot + visibility_index
     → 校验 sha256 + min_reader_version
     → 设为 active_buffer
     → readiness = true

后台 watcher（inotify 或 500ms 轮询 manifest）
     → 发现 version 递增
     → mmap staging 文件
     → 校验通过 → 原子切换 active_buffer 指针
     → 旧 buffer 延迟 unmap（quiesce 后）
```

下架优先：`visibility_index` 独立 bundle 可先切换，再等待 `catalog_snapshot` 全量。

## 2. 首页推荐（读）

```text
Client → gateway → user-server(可选)
       → recommendation-server.QueryRecommendations
            → catalog_read：候选 ID 集合（snapshot 索引）
            → visibility_index：过滤
            → recommendation：排序 + 解释
       → gateway 聚合（可内联 BatchGetGuideCards 字段，减少往返）
```

`user-server` 超时：降级热门（`40003`），不穿透运营库。

## 3. 导购详情（读）

```text
gateway → recommendation-server.BatchGetGuideCards(ids)
        → 仅 mmap 命中且 visible 的卡片
        → 含 governance 披露标签（已物化在 snapshot）
```

## 4. 降级策略

| 条件 | 行为 |
|------|------|
| snapshot 不可用 | readiness 失败；gateway 收 `10052` |
| 候选池空 | 回退 `GetPopularRecommendations`（snapshot 内预计算热门） |
| visibility 不确定 | fail-closed，不返回该条 |

## 5. 与 backoffice 发布联动

本服务**不**订阅 Kafka 做在线读。新鲜度完全由 manifest 版本驱动；可选 Kafka 仅触发预热指标，不改变读权威。
