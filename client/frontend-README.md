# 前端详细设计总览

## 0. 商业化交付优先级（2026-05）

- **主交付**：微信小程序（[wechat-miniprogram/README.md](./wechat-miniprogram/README.md)）。
- **第二阶段**：iOS / Android 原生 App 按同一 gateway 契约补齐真实网络层、登录、收藏、历史与跳转。
- **Web C 端**：不与运营后台混淆，若启动则作为独立 C 端渠道接入同一 gateway。

## 1. 目标

本目录定义 `少糖` 在 Web、iOS、Android、微信小程序的前端详细设计，确保：

- 交互逻辑简单高效
- 产品语义跨端一致
- UI 简洁大方、长期可用
- 前后端可并行开发

## 2. 文档索引

- [前端设计原则](./frontend-principles.md)
- [信息架构与核心流程](./frontend-information-architecture.md)
- [页面规格与状态定义](./frontend-page-specs.md)
- [登录交互逻辑](./frontend-login-interaction.md)
- [登录错误文案与 CTA](./frontend-login-error-copy.md)
- [前端跨端交互共识（全局）](./cross-platform-interaction-consensus.md)
- [iOS 交互实现说明](./ios/interaction-notes.md)
- [Android 交互实现说明](./android/interaction-notes.md)
- [微信小程序](./wechat-miniprogram/README.md) · [分端交互说明](./wechat-miniprogram/interaction-notes.md)
- Web C 端实现：当前无开发计划（与运营后台 `backoffice-web` 无关，后者见 `services/platform/docs/`）
- [前端与 Gateway 交互设计](./frontend-gateway-interaction.md)
- [三端实现映射](./frontend-platform-mapping.md)
- [测试夹具与验收矩阵](./frontend-mock-and-acceptance.md)
- [Gateway C 端 E2E 清单](./tests/gateway-e2e-checklist.md)

## 3. 维护矩阵（改契约时同步更新）

| 变更类型 | 必改文档 |
|----------|----------|
| 页面 IA / 流程 | `frontend-information-architecture.md`、`frontend-page-specs.md`、各端 `interaction-notes.md` |
| 登录 / 错误文案 | `frontend-login-interaction.md`、`frontend-login-error-copy.md` |
| Gateway JSON 字段 | `.cursor/rules/shared-contracts.mdc`、`frontend-gateway-interaction.md`、`services/gateway/docs/api.md` |
| 三端差异 | `frontend-platform-mapping.md`、`cross-platform-interaction-consensus.md` |
| C 端 API 烟雾 / E2E | `client/tests/gateway_api_smoke.py`、`tests/gateway-e2e-checklist.md` |
| 验收 / mock（UI 五态） | `frontend-mock-and-acceptance.md` |

## 4. 与顶层文档关系

- 对外品牌名：`README.md`（品牌与命名章节）
- 顶层设计与架构：`README.md` 与 `services/README.md`
- 公共 JSON 契约：`.cursor/rules/shared-contracts.mdc`
- Gateway 对外 API：`services/gateway/docs/api.md`

## 5. 单一事实来源

- 前端页面交互、状态与体验规则以本目录文档为准
- JSON 字段语义以 `.cursor/rules/shared-contracts.mdc` 和 `services/gateway/docs/api.md` 为准
- 若文档冲突，先修正文档再修正实现

## 6. 品牌与公共资源

跨端共用的品牌素材放在 `client/assets/`，各端从此导出平台规格（App Icon、启动图等），勿在各端目录各存一份源文件。

| 路径 | 说明 |
|------|------|
| [`assets/README.md`](./assets/README.md) | 品牌素材清单（含 logo 方/圆版） |
