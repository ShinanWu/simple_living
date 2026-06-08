# backoffice-web

运营管理平台 **UI**（React + Vite + TypeScript SPA）。

产品与 design 见 **[`../README.md`](../README.md)**（从 [详细设计](../detail-design.md) 查看页面与 API 映射）。

## 代码

| 路径 | 说明 |
|------|------|
| `src/App.tsx` | 入口，构造 `HttpGatewayApiClient` |
| `src/pages/OperationsConsolePage.tsx` | 运营台主页面 |
| `src/gateway/client.ts` | gateway HTTP 客户端 |
| `src/gateway/types.ts` | 契约类型镜像 |

## 本地开发

```bash
npm install
npm run dev          # http://127.0.0.1:5173/backoffice/
npm run typecheck && npm run test && npm run build
```

- `base = /backoffice/`（与 nginx 反代一致）
- 生产构建：`VITE_GATEWAY_BASE_URL` 留空，同源走 `/api/v2/backoffice/*`

## 部署

```bash
bash deploy/deploy_service.sh
```

详见 [deploy/README.md](deploy/README.md) 与 [../README.md](../README.md) §部署。

镜像：`simple-living-backoffice-web:<tag>`，容器 nginx 监听 **8088**。
