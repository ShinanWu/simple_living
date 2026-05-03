# 运营后台平台（Web）v1 交付方案

## 1. 目标与范围

本方案定义一版“可落地、可演进”的运营后台平台（网页），用于承接以下域能力：

- `affiliate-domain`：伙伴注册、佣金规则与异常对账运营动作
- `content-domain`：专题/榜单/卡片内容编排与发布动作
- `governance-domain`：审核、可见性、披露与风险标记动作

v1 聚焦最小闭环：**运营可查看核心列表并完成关键动作**，复杂筛选、批量任务与审批流作为后续迭代。

## 2. 架构与边界

- 对外入口仍为 `gateway`（HTTPS + JSON）。
- 域内业务规则仍归各域所有，运营后台只做页面编排与操作触发。
- 网页端不直连域服务内部 proto。
- 网关作为 BFF 负责 JSON 到内部 RPC 映射。

## 3. 信息架构（v1）

一级导航（Web）：

1. `后台首页`（后续迭代，可先留空）
2. `联盟管理`（affiliate）
3. `内容管理`（content）
4. `治理审核`（governance）

本次实现优先覆盖后 3 个模块。

## 4. 页面与最小动作

### 4.1 联盟管理（affiliate）

- 伙伴列表（名称、状态、主渠道）
- 新增伙伴（最小字段：`partner_id`、`display_name`、`status`）
- v1 不处理密钥录入，仅记录业务元信息

### 4.2 内容管理（content）

- 内容条目列表（`content_id`、标题、主题、发布态）
- 发布/下架切换（最小布尔态）
- v1 不内嵌富文本编辑器

### 4.3 治理审核（governance）

- 待审队列（`review_id`、目标、状态）
- 通过/拒绝动作（最小文本意见可选）
- v1 不做批量审核

## 5. 前端工程落地（v1）

- 目录：`services/platform/backoffice-web`
- 详细设计文档：`services/platform/backoffice-web/docs/detail-design.md`
- 测试与验收计划：`services/platform/backoffice-web/docs/test-plan.md`
- 新增页面：`OperationsConsolePage`
- 数据访问：
  - `GatewayApiClient` 增加后台接口
  - `FakeGatewayRepository` 提供可交互 mock 数据
- 运行模式：
  - 默认 mock（可直接演示）
  - 后续可切换 `HttpGatewayApiClient` 联调真实网关

## 6. TDD 与测试策略

最小单元测试覆盖：

- App 导航能进入后台页
- 后台页默认渲染三个模块入口
- 联盟管理新增伙伴后列表更新
- 治理审核状态变更后界面反映

执行命令：

```bash
cd services/platform/backoffice-web
npm run test
```

## 7. 第二版迭代计划

- 补充字段级校验与错误提示
- 增加筛选、分页、详情抽屉
- 对接真实网关路由并补齐契约示例
- 扩展单测到错误态与并发动作

## 8. 第三版部署与链路

- 为 `services/platform/backoffice-web` 增加 Docker 镜像构建
- 在 `infra/lab` 补充平台启动与验证步骤
- 增加“后台页 -> 网关 -> 域服务”的冒烟链路脚本

## 9. 验收标准（v1）

- 页面可运行、可交互
- 三个模块最小动作可完成
- 单元测试通过
- 文档与实现一致
