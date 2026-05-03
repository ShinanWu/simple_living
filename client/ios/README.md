# iOS Scaffold (SwiftUI)

This directory provides a minimal iOS scaffold aligned with `client/frontend-*.md` for parallel development.

## Scope

- App shell with 2 tabs: `首页` and `我的`
- Home top segmented tabs mapped to `clothing`, `food`, `housing`, `transport`
- Shared page state enum: `loading`, `success`, `empty`, `error`, `offline`
- Core flow screens: Home -> GuideDetail -> RedirectPrepare, plus MeSummary
- Gateway API protocol with:
  - `MockGatewayAPI`（默认，无环境变量时）
  - `HttpGatewayAPI`（真实 HTTPS + JSON，对齐 `services/gateway/docs/api.md`）
- 我的页：`登录` sheet（手机号 + 微信联调字段）、`退出`（`DELETE /api/v2/auth/session`）；令牌落盘 `UserDefaults`，冷启动自动恢复
- `GatewayRuntime.makeFromEnvironment()` 在 App 入口选择实现

## Structure

- `Sources/App`: app entry and tab shell
- `Sources/Features`: feature modules by page（SwiftUI）
- `Packages/SimpleLivingCore`: 本地 Swift Package（`SimpleLivingCore` 模块：Gateway、ViewModel、共享模型）；与 App 分离，避免 Xcode 把 `relativePath = .` 指到 `.xcodeproj` 旁时的 **Missing package product** 问题

## Run the app (Xcode)

1. 安装 [XcodeGen](https://github.com/yonaskolb/XcodeGen)（一次性）：`brew install xcodegen`
2. 在 `client/ios` 执行：`./generate_project.sh`（或等价：`xcodegen generate && python3 scripts/fix_local_spm_package_link.py`）
   - **必须两步都执行**：XcodeGen 不会把 `package = <XCLocalSwiftPackageReference>` 写进 `XCSwiftPackageProductDependency`；修复脚本已针对真实 product 块（避免误改 `PBXFileReference` 同名项）。
3. 打开生成的 `SimpleLiving.xcodeproj`，选择 `SimpleLiving` scheme，在模拟器运行

未安装 XcodeGen 时，可在 Xcode 中新建 iOS App 工程，将 `Sources/App` 与 `Sources/Features` 加入 target，并添加本地 Swift Package 依赖（路径为 `client/ios/Packages/SimpleLivingCore`，product `SimpleLivingCore`）。

## 联调真实 Gateway

在 Xcode scheme 的 **Environment Variables** 中设置（或使用启动参数等价配置）：

| 变量 | 说明 |
|------|------|
| `GATEWAY_BASE_URL` | 网关根地址，如 `https://api.example.com`（勿带末尾路径 `/api/...`） |
| `GATEWAY_ACCESS_TOKEN` | 可选，已登录时 `Bearer` 对应 token（通常不设，改用 App 内登录后的持久化） |
| `GATEWAY_REFRESH_TOKEN` | 可选，覆盖本地持久化的 refresh token（一般不设） |
| `GATEWAY_GUEST_SESSION_ID` | 可选，已有访客会话时可注入，否则客户端会对 `POST /api/v2/guest/session` 懒创建 |
| `GATEWAY_APP_VERSION` | 可选，写入请求体中的 `app_version` / `request_context.app_version`，默认 `0.1.0` |

未设置 `GATEWAY_BASE_URL` 时使用 **Mock**，便于离线跑通 UI。

**说明**：`redirect_prepare` 与 `me_summary` 需要登录态或访客会话；HTTP 客户端会在首次需要时申请访客会话并缓存 `X-Guest-Session-Id`。

若使用 **HTTP（非 HTTPS）** 本机调试，需在 Info.plist 中配置 ATS 例外（Xcode 里 `App Transport Security`），生产环境应使用 HTTPS。

## Self-check（核心库）

```bash
cd client/ios
swift test --package-path Packages/SimpleLivingCore
```

## iOS first rollout status

- Shared contracts: ready
- iOS flow state machine: ready
- iOS core tests: ready（`swift test --package-path Packages/SimpleLivingCore`）
- Real gateway HTTP client: **ready**（`HttpGatewayAPI` + `GatewayRuntime`）
- Xcode 工程: 通过 `project.yml` + XcodeGen 生成

## Notes

- `HttpGatewayAPI` 使用 `JSONEncoder/JSONDecoder` 的 snake_case 转换，与 `docs/contracts/` 字段一致。
- 首页列表项使用 `guide_card_id` 进入详情；`redirect_prepare` 携带 `FeedItemContext`（含 `recommendation_id` / `scene` / `item_rank`）。
- Gateway remains the only client entry; no internal `proto` assumptions in iOS.
