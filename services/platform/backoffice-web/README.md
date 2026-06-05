# Backoffice Web (`services/platform/backoffice-web`)

运营管理后台 Web（platform 侧实现目录），用于承载内容运营、联盟伙伴、治理审核与审计视图。运行时默认连接真实 gateway，由 gateway 统一编排 affiliate/content/governance 等领域服务。

## Included

- 平台侧运营页：`OperationsConsole`（联盟管理 / 内容管理 / 治理审核）
- Gateway backoffice 契约调用（v3 完整能力，**只引用不复制契约**，权威见 `../../gateway/docs/backoffice-backend.md` 与 `../../gateway/docs/api.md` §13.6）：
  - 联盟：`affiliate/partners`、`affiliate/partners/add`
  - 内容：`content/items`、`.../add`、`.../update`、`.../detail`、`.../submit-review`、`.../publish`、`.../status`、`.../rollback`
  - 治理：`governance/reviews`、`.../reviews/status`、`governance/visibility`
- `FakeGatewayRepository` 仅用于组件测试，不作为开发、演示或验收运行时
- 服务总览、SLO、配置、部署、可观测性、安全详见 `docs/README.md`

## Docs

- `docs/README.md`
- `docs/detail-design.md`
- `docs/test-plan.md`

## Structure

- `src/App.tsx`: backoffice 应用入口，默认实例化 `HttpGatewayApiClient`
- `src/pages/OperationsConsolePage.tsx`: 运营管理台页面
- `src/gateway/`: backoffice API types / HTTP client / test-only fake repository

## Run now

```bash
npm install
npm run dev
```

## Self-check

```bash
npm run typecheck
npm run test
npm run build
```

## Commercial Roadmap

1. 接入 backoffice 登录、角色与数据权限，由 `user-server` / `platform/backoffice-backend` 提供操作者身份与授权裁决。
2. 将内容编辑、审核、发布、下架、回滚、审计原因统一落到 gateway backoffice API。
3. 补齐运营审计链路（actor / reason / request_id / trace_id）并进入 PostgreSQL / Kafka outbox。
4. 单服务部署入口：`deploy/README.md` 与 `deploy/deploy_service.sh`。
