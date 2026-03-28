# 服务契约总览

## 1. 目的

本目录只用于沉淀 **跨服务、跨终端共享的稳定公共契约**，作为并行开发时的统一协作协议。

它不是所有业务 API 的集中存放处，也不是各服务 `proto` 的替代品。

## 2. 二层契约模型

本项目的契约分为两层：

### 第一层：服务私有契约

每个服务自己维护并对自己的接口负责，包括：

- `services/<service>/docs/api.md`
- `services/<service>/docs/README.md`
- `services/<service>/docs/data-model.md`
- `services/<service>/proto/` 或对应服务目录下的 `proto/`

规则如下：

- 谁提供服务，谁维护接口定义和 `proto`
- 调用方依赖提供方发布的契约，不复制、不私改一份本地版本
- 一个结构如果只在单个服务内部或单个服务对外接口中使用，应留在该服务内维护

### 第二层：全局公共契约

`docs/contracts/` 只维护真正跨服务复用的协议，例如：

- 通用响应结构
- 错误码规范
- 分页协议
- 鉴权约定
- 导购卡片通用结构
- 推荐结果公共结构
- 跳转归因公共字段语义

一个结构只有在 **多个服务、多个终端或多个业务域** 都需要共同理解时，才应该提升到本目录。

## 3. JSON 与 proto 的关系

本目录定义的是 **对外 JSON 语义和跨服务公共字段语义**，不是内部 `proto` 的替代品。

规则如下：

- 客户端可见的公共 JSON 结构以 `docs/contracts/` 为准
- 各服务内部 RPC 消息由服务 owner 自己维护 `proto`
- `gateway` 负责将公共 JSON 契约映射到内部 `proto`
- 若 JSON 契约与内部 `proto` 出现冲突，必须先修改文档并统一映射层，不能临时各自解释

## 4. 什么应该放在这里

以下内容适合放在 `docs/contracts/`：

- 平台级通用协议
- 多服务共享字段字典
- 多端统一消费的响应结构
- 需要长期保持稳定的公共语义

以下内容不应该放在 `docs/contracts/`：

- 某个服务独有的业务 API
- 某个服务内部实现细节
- 某个服务专属的 `proto` 全量定义
- 只被单一调用方使用的临时结构

## 5. 当前公共契约清单

以下文档已落地，JSON 字段统一使用 **`snake_case`**，示例与 [common-response.md](./common-response.md) 信封一致：

| 文档 | 说明 |
|------|------|
| [common-response.md](./common-response.md) | 通用 API 响应信封（`success` / `code` / `message` / `data` / `meta` 等） |
| [error-codes.md](./error-codes.md) | 按业务域划分的整数错误码区间与常用语义 |
| [pagination.md](./pagination.md) | 偏移分页与游标分页的请求参数及 `data.items` + `data.pagination` 结构 |
| [auth.md](./auth.md) | 终端与网关的鉴权头、令牌角色、访客/登录用户态（不涉及实现细节） |
| [guide-card.md](./guide-card.md) | 导购内容卡片 canonical schema（含商业披露与联盟上下文占位） |
| [recommendation.md](./recommendation.md) | 推荐结果、`scene` 枚举及条目与导购卡片的引用关系 |
| [redirect-attribution.md](./redirect-attribution.md) | 点击 ID、跳转落地链、渠道与转化回传字段语义 |

## 6. 调用与依赖原则

服务之间的调用遵循以下原则：

1. 提供方服务维护自己的接口文档和 `proto`
2. 调用方服务依赖提供方发布的接口契约
3. 调用方不得复制一份新的 `proto` 并自行演化
4. 如果多个服务都反复出现同一公共结构，再将该结构上升为 `docs/contracts/` 中的公共契约

## 7. 管理原则

- 契约变更必须先改文档
- 契约变更必须记录兼容性影响
- 公共契约只能新增兼容性字段，禁止无提示破坏性修改
- 服务私有契约变更由服务 owner 负责
- 公共契约变更需要考虑所有消费方的兼容性

## 8. 推荐目录

```text
docs/contracts/
├── README.md
├── common-response.md
├── error-codes.md
├── pagination.md
├── auth.md
├── guide-card.md
├── recommendation.md
└── redirect-attribution.md
```
