# governance-domain — 工作流

## 1. 内容审核主流程

```text
[内容变更] (content-domain)
       │
       ▼
  入审 Enqueue ──► ReviewQueueItem (pending)
       │
       ├──► 审核员认领 / 分配 (可选)
       │
       ▼
  策略评估 policies:evaluate (可选，自动预审提示)
       │
       ▼
  SubmitReviewDecision ──► ReviewDecision
      │
      ├── outcome=rejected ──► 建议触发 VisibilityVerdict unpublished（可自动或人工确认）
      ├── outcome=approved ──► 记录“审核通过”，等待运营显式发布
      └── outcome=needs_info ──► 队列挂起，通知内容方补充
```

**并行开发约定**：v1 默认 **过审不自动上线**，仍需运营显式发布；若后续开启自动上线，必须通过 `ops-config` 灰度放开并记录变更。

---

## 2. 发布 / 下架与内容编辑的关系

本项目在 v1 默认采用 **A. 治理裁决为准**：

| 模式 | 说明 |
|------|------|
| **A. 治理裁决为准** | `content-domain` 表示内容是否准备好发布；对用户是否真正可见，以 `VisibilityVerdict.state` 为最终准入条件。 |

规则如下：

- `content-domain` 可维护 `draft`、`in_review`、`ready_for_publish`、`offline` 等编辑态
- 任何面向用户的展示链路，必须满足 `VisibilityVerdict.state = published`
- `VisibilityVerdict.state != published` 时，推荐、gateway、tracking 均不得向用户暴露该内容

---

## 3. 商业合作标识流程

```text
运营在后台绑定 CooperationLabel
       │
       ▼
  校验 policies:evaluate（如：某合作类型必须选披露模板）
       │
       ▼
  持久化 + 事件 governance.cooperation.label_updated
       │
       ▼
  BFF 组装 guide-card / 详情页披露文案
```

**并行点**：content-domain 不必理解合作类型枚举；枚举字典可由本域或配置服务下发。

---

## 4. 风险标记与审核/可见性

```text
RiskFlag 创建/升级 (人工或外部引擎回调)
       │
       ├──► 可选：自动提高 ReviewQueueItem.priority
       ├──► 可选：policies 要求 unpublished 直至复核
       └──► 事件 governance.risk.flag_changed
```

解除标记或过期后，应再次 `visibility:evaluate` 或依赖订阅方刷新缓存。

---

## 5. 运营配置发布流程

```text
草稿编辑 ──► 校验（Schema）──► 发布 OpsConfigRelease
       │
       ▼
  事件 governance.ops_config.released
       │
       ▼
  各域拉取或推送更新（热加载 / 本地缓存 TTL）
```

治理相关 **功能开关**（如是否启用某类自动下架）建议走同一发布管线，便于审计。

---

## 6. 策略执行在链路中的位置

| 阶段 | 调用方 | 用途 |
|------|--------|------|
| 入审前 | content-domain 或网关 | 拦截明显违规，减少人工队列 |
| 审核中 | 审核台 | 展示命中规则与依据 |
| 展示前 | recommendation-domain / BFF | `visibility:evaluate`；**不在本域内排序** |
| 跳转前 | tracking BFF（可选） | 核对披露完备性，不满足则拒绝生成链接（业务规则产品定） |

---

## 7. 与 recommendation-domain 的协作（边界重申）

1. recommendation-domain 负责 **候选集与排序**。
2. governance-domain 提供 **允许展示** 与 **必须披露** 等约束；推荐服务在 **召回后、响应前** 调用判定或消费快照。
3. 若需「降权」而非硬过滤，可在契约中约定 **治理分数** 由本域输出、推荐域 **仅作为特征** 使用；本域仍不实现排序公式。

---

## 8. 审计与追责

- 所有 **VisibilityVerdict**、**ReviewDecision**、**CooperationLabel** 变更建议追加 **审计日志**（谁、何时、旧值、新值、原因）。
- 对外合规请求时，以 **审计 ID + 时间范围** 导出。

---

## 9. 失败与降级

- 推荐主链路默认优先消费 **物化可见性快照**；仅后台、低频校验或兜底路径调用 `visibility:evaluate`。
- `visibility:evaluate` 超时：默认 **fail-closed** 不展示，避免合规风险扩大。
- 配置中心不可用：使用 **上次成功快照**；无快照时行为在 `ops-config` 默认值中定义。
