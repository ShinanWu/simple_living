# 前端详细设计总览

## 1. 目标

本目录定义 `简单生活` 在 Web、iOS、Android 三端的前端详细设计，确保：

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
- Web C 端实现：当前无开发计划（避免与运营后台实现边界混淆）
- [前端与 Gateway 交互设计](./frontend-gateway-interaction.md)
- [三端实现映射](./frontend-platform-mapping.md)
- [Mock 数据与验收矩阵](./frontend-mock-and-acceptance.md)
- [运营后台平台 v1 方案](./backoffice-platform-v1.md)

## 3. 与顶层文档关系

- 顶层设计与架构：`docs/`
- 公共 JSON 契约：`docs/contracts/`
- Gateway 对外 API：`services/gateway/docs/api.md`

## 4. 单一事实来源

- 前端页面交互、状态与体验规则以本目录文档为准
- JSON 字段语义以 `docs/contracts/` 和 `services/gateway/docs/api.md` 为准
- 若文档冲突，先修正文档再修正实现
