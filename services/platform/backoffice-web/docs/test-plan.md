# backoffice-web 测试与验收计划（v2）

## 1. 测试范围

- 页面渲染与交互（unit/component）
- 网关契约调用与错误处理（integration-lite）
- QEMU 部署链路（smoke）

## 2. 单元测试最低覆盖

1. 页面可渲染三域 tab
2. 联盟新增成功后列表更新
3. 内容状态切换后列表更新
4. 审核通过/拒绝后列表更新
5. 接口失败时展示错误提示并允许重试
6. 操作日志追加并包含 `request_id`

## 3. 手工回归清单

- 首次加载：三域列表均可见
- 切 tab：状态不串扰
- 新增伙伴：输入合法时成功、非法时阻止提交
- 内容状态切换：按钮文案随状态变化
- 审核动作：仅 `pending` 项显示操作按钮
- 错误恢复：失败后可点击刷新或再次提交

## 4. QEMU 验收步骤

1. 节点启动与检查
   - `bash services/platform/backoffice-web/deploy/start_nodes.sh`
   - `bash services/platform/backoffice-web/deploy/check_nodes.sh`
2. 构建并部署
   - `bash services/platform/backoffice-web/deploy/deploy_service.sh`
3. 基础可达性
   - `curl -fsS "http://127.0.0.1:18081" | sed -n '1,10p'`
4. 网关联通性（至少命中一个 backoffice API）
   - 页面执行“新增伙伴”并确认列表更新
   - 记录一次 `request_id` 用于追踪

## 5. 交付门槛

- 文档与实现一致
- 单元测试通过（在可用 Node 环境）
- QEMU 部署与基础访问通过
- 至少完成三域各一条真实可操作路径
