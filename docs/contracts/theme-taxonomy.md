# 主题分类契约（theme taxonomy）

## 1. 目的

定义跨端（Web/iOS/Android）与 gateway 共享的主题枚举语义，避免各端自行维护映射导致并行开发偏差。

## 2. 适用范围

- 首页四主题 Tab
- 推荐查询中的 `theme` 过滤参数
- 与主题相关的埋点、缓存键、页面恢复逻辑

## 3. 枚举定义

JSON 字段统一使用 `snake_case`。主题枚举值如下：

| 中文主题 | `theme` 值 | 说明 |
|---------|------------|------|
| 衣 | `clothing` | 穿搭、服饰、鞋包、通勤着装等 |
| 食 | `food` | 餐饮、食材、健康饮食、外卖决策等 |
| 住 | `housing` | 家居、租住、家电、收纳清洁等 |
| 行 | `transport` | 出行方式、通勤工具、旅行交通等 |

## 4. 兼容性与演进

- 新增主题只能向后兼容追加，禁止重定义既有值语义
- 已发布值禁止静默删除或改名
- 若新增主题，需同步更新：
  - `services/gateway/docs/api.md`
  - `client/frontend-information-architecture.md`
  - `client/frontend-page-specs.md`
  - 对应前端埋点与测试矩阵

## 5. 前端实现约束

- UI 文案允许本地化（如“衣/食/住/行”），但请求参数必须使用本契约枚举值
- Tab 排序默认固定为：`clothing` -> `food` -> `housing` -> `transport`
- 缓存键建议包含 `theme`，避免不同主题列表数据串用
