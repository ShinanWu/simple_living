# Backoffice Web (`services/platform/backoffice-web`)

运营管理后台 Web（platform 侧实现目录），用于承载 affiliate/content/governance 三域最小运营动作。

## Included

- 平台侧运营页：`OperationsConsole`（联盟管理 / 内容管理 / 治理审核）
- Gateway backoffice 契约调用：
  - `GET /api/v2/backoffice/affiliate/partners`
  - `POST /api/v2/backoffice/affiliate/partners/add`
  - `GET /api/v2/backoffice/content/items`
  - `PATCH /api/v2/backoffice/content/items/status`
  - `GET /api/v2/backoffice/governance/reviews`
  - `PATCH /api/v2/backoffice/governance/reviews/status`
- `FakeGatewayRepository` 支持本地最小联调

## Docs

- `docs/README.md`
- `docs/detail-design.md`
- `docs/test-plan.md`

## Structure

- `src/App.tsx`: backoffice 应用入口（当前直接渲染运营控制台）
- `src/pages/OperationsConsolePage.tsx`: 运营管理最小页
- `src/gateway/`: backoffice API types / HTTP client / fake repository

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

## Next integration steps

1. Switch from `FakeGatewayRepository` to `HttpGatewayApiClient`.
2. 增加权限模型与操作审计展示（actor / reason / request_id）。
3. 补齐 backoffice 权限/审计链路联调用例。
4. 参考 `deployment.md` 进行 Docker 与 QEMU 部署。
5. 单服务部署入口：`deploy/README.md` 与 `deploy/deploy_service.sh`。
