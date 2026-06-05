# backoffice-web 服务总览（上线级）

`services/platform/backoffice-web` 是 **运营管理后台 Web**（React + Vite + TypeScript 单页应用）。它是运营人员在内容、治理、联盟配置上执行日常运营动作的工作台，**运行时默认连接真实 `gateway`**，所有业务真相经 `gateway` 的 backoffice API 落到 `platform/backoffice-backend`，本服务自身不持久化业务数据。

本目录是 `backoffice-web` 的单一事实来源。设计细节见 `detail-design.md`，测试与上线门槛见 `test-plan.md`，变更记录见 `changelog.md`。

## 1. 服务目标

- **列表可读**：运营快速查看三域当前业务状态。
- **动作可执行**：核心运营操作可在页面内直接完成并即时反馈结果。
- **结果可追溯**：每次写操作展示后端回传的 `request_id` / `trace_id` 并写入「最近操作日志」。
- **失败可恢复**：接口失败时保留输入、可重试，单模块失败不阻塞其他模块。
- **真实闭环**：默认连真实 `gateway`，`mock`（`FakeGatewayRepository`）仅作组件测试夹具，不进入开发演示与验收运行时。

## 2. 覆盖的运营动作

| 域 | 运营动作 | 对应 gateway backoffice 路由（引用，不复制契约） |
|------|----------|--------------------------------------------------|
| 联盟（affiliate） | 伙伴列表、新增伙伴 | `affiliate/partners`、`affiliate/partners/add` |
| 内容（content） | 列表/过滤、新增、编辑、详情（含版本历史）、提交审核、发布、状态切换（下架/归档/重发）、版本回滚 | `content/items`、`.../add`、`.../update`、`.../detail`、`.../submit-review`、`.../publish`、`.../status`、`.../rollback` |
| 治理（governance） | 审核队列、审核裁决（通过/拒绝/补充材料）、可见性裁决 | `governance/reviews`、`.../reviews/status`、`governance/visibility` |
| 审计 | 展示并查询「最近操作日志」（action / target / result / request_id / timestamp） | 复用各写接口响应的 `meta.request_id` / `meta.trace_id` |

> 风险/配置类运营动作（强制下架、策略开关等）在 v1 经治理「可见性裁决」表达；独立的风险控制台与配置中心为 **v1 范围外**，占位行为：以 `governance/visibility` 的 `restricted` / `unpublished` 实现紧急不可见。

完整字段、状态机、交互约束见 `detail-design.md`。

## 3. 职责与非职责

**本服务负责（前端编排与呈现）**

- 三域运营页面的信息架构、表单校验、状态流转引导、操作反馈与审计展示。
- 调用 `gateway` backoffice API，并按约定信封（`success/code/message/data/meta`）解析与错误分类。

**本服务不负责（指向正确归属）**

- 业务真相与持久化：归 `platform/backoffice-backend`（经 `gateway` `/api/v2/backoffice/*`）。
- JSON ↔ proto 映射、跨域编排、鉴权注入：归 `gateway`（见架构边界规则）。
- 审核是否自动发布的策略：v1 审核通过 **不自动发布**，由治理可见性裁决最终决定，本服务只触发显式发布。

## 4. 上下游依赖（只引用契约）

- **唯一上游：`gateway` backoffice API。** 路由与权限总表见 `../../../gateway/docs/api.md` §13.6；运营后台后端能力、数据对象、请求/响应体、状态机与权限模型见 `../../../gateway/docs/backoffice-backend.md`（**本服务的 backoffice 契约权威来源**）。
- **公共 JSON 语义**（响应信封、错误码区间、分页、鉴权头）：`.cursor/rules/shared-contracts.mdc`。
- 本服务 **不** 直接依赖任何域服务实现目录或内部 `proto`；契约或字段变更时先改 `gateway` 文档，再改本目录与实现。

> 契约口径说明：`gateway/docs/api.md` §13.6 为路由摘要（含最小集合），完整 v3 backoffice 能力（`update/detail/submit-review/publish/rollback/visibility`）以 `gateway/docs/backoffice-backend.md` 为准；如二者出现差异，以 backoffice-backend.md + 该域 `proto` 为准并先行修订 gateway 文档。

## 5. 服务等级目标（SLO）

本服务是静态前端资产 + 浏览器内编排，自身不承载业务 RPC，因此 SLO 聚焦「资源可用性」与「关键运营操作成功率（端到端含 gateway）」：

| 指标 | 目标 | 口径 |
|------|------|------|
| 静态资源可用性（页面可达） | 月度 ≥ 99.9% | `GET /backoffice/`（或容器 `:8088/`）返回 200 且加载 `index.html` |
| 首屏可交互（本地/同城网络） | P50 ≤ 1.5s，P95 ≤ 3s | 首次三域并发加载完成、页面进入 `ready` |
| 关键运营操作端到端成功率 | ≥ 99%（剔除运营输入错误 `10002`/`10003`/`10004`） | 新增/编辑/提交审核/发布/审核裁决/可见性，统计 `success===true` 占比 |
| 关键操作端到端延迟 | P95 ≤ 2s | 点击到收到 gateway 响应并更新列表 |

> SLO 的网关侧依赖（域服务可用性、写入延迟）以 `gateway` 与各域 `docs/README.md` 的 SLO 为准，本服务不重复承诺。

## 6. 构建 / 运行 / 配置

### 6.1 本地开发与构建

```bash
npm install            # 安装依赖（Node 20）
npm run dev            # 本地开发服务器（Vite，默认 http://127.0.0.1:5173/backoffice/）
npm run typecheck      # tsc 类型检查
npm run test           # vitest 组件/契约-lite 测试
npm run build          # 生产构建：tsc -b && vite build → dist/
npm run preview        # 本地预览生产产物
```

- Vite `base = /backoffice/`：所有静态资源与路由前缀均为 `/backoffice/`，与反代/部署路径一致。
- 产物为纯静态文件（`dist/`），由静态服务器（`serve` 或 nginx）托管，无服务端运行时。

### 6.2 配置项

前端为静态构建，配置以 **构建期注入** 为主（Vite 在 build 时内联 `import.meta.env.VITE_*`）。

| 配置项 | 类型 | 默认值 | 必填 | 说明 |
|--------|------|--------|------|------|
| `VITE_GATEWAY_BASE_URL` | 构建期 env | `http://8.152.103.12`（`src/App.tsx` 兜底） | 否 | 真实 `gateway` 的 base URL。**生产推荐留空（同源）**，由反代把 `/api/v2/backoffice/*` 转发到 gateway，避免把网关地址固化进静态产物，也规避跨域。 |
| 容器对外端口 | 部署参数 | `8088` | 否 | `Dockerfile` 用 `serve -s dist -l 8088` 托管；可在 `docker run -p` 改宿主端口。 |
| `base`（路由前缀） | 构建期常量 | `/backoffice/` | 否 | 在 `vite.config.ts` 固定；改前缀需同步反代 `location`。 |
| `FakeGatewayRepository` 开关 | 仅测试 | 关闭（不在运行时启用） | 否 | 仅 `*.test.tsx` 注入；运行时一律使用 `HttpGatewayApiClient`（连真实 gateway）。 |

> 运行时切换网关地址：因 `import.meta.env` 为构建期值，**不重新构建即切换网关** 的推荐做法是「同源 + 反代」（base URL 置空），由部署侧 nginx 决定后端；需直连跨域网关时再用 `VITE_GATEWAY_BASE_URL` 重新构建。

### 6.3 登录 / 鉴权方式与角色

- **身份注入归 `gateway`**：backoffice 路由要求 `Authorization: Bearer <token>`（运营角色），由 gateway 校验（见 `gateway/docs/api.md` §13.6 与 `backoffice-backend.md` §7）。
- **业务角色**（后端口径，前端按返回结果约束，不做强前端判权）：`backoffice_admin`（全部）、`content_operator`（内容创建/编辑/提交审核/发布下架）、`reviewer`（审核与可见性裁决）、`partner_operator`（伙伴管理）。
- **token 处理（现状与演进）**：v1 前端 **不持久化 token**，依赖「受限网络 / 反代鉴权 + gateway 运营角色校验」保护（占位上线姿态）；将登录态接入前端、由 `user-server` / `platform/backoffice-backend` 签发并在 `HttpGatewayApiClient` 注入 `Authorization` 头，为下一阶段演进项（接入点：`src/gateway/client.ts` 的请求头与 `src/App.tsx` 的客户端构造）。该项为 **v1 范围外但路径已定**，不阻塞按本文档开发与上线。

## 7. 部署

容器化静态托管，单服务独立部署入口在 `../deploy/`（含本地 Docker 与 QEMU lab 步骤）。

- **构建产物**：`dist/`（纯静态）。镜像 `simple-living-backoffice-web:<tag>`，默认 `latest`。
- **托管方式**：容器默认 `serve -s dist -l 8088`（`Dockerfile`）。如需 nginx 托管，仓库内提供 `../nginx.conf`（`listen 8088`，SPA `try_files` 回退 `index.html`）作为等价静态托管配置。
- **环境变量注入**：`VITE_GATEWAY_BASE_URL` 于 **构建期** 生效（写入镜像的静态产物）；运行期仅暴露端口，不读业务 env。生产入口建议由前置 nginx 反代：`/backoffice/` → 本服务，`/api/v2/backoffice/*` → gateway（见 `test-plan.md` §7 验收记录）。
- **部署命令**（仅本服务，不会构建/发布其他服务）：

```bash
bash ../deploy/start_nodes.sh
bash ../deploy/check_nodes.sh
bash ../deploy/deploy_service.sh
```

- **回滚**：以指定镜像 tag 重新部署即可（容器以 `--restart unless-stopped` 运行）：

```bash
IMAGE_TAG=v2026.04.27 bash ../deploy/deploy_service.sh
bash ../deploy/stop_nodes.sh   # 需要停服时
```

- **验收**（静态资源可达 + 至少命中一个 backoffice API）：

```bash
curl -fsS "http://127.0.0.1:18081" | sed -n '1,10p'   # 页面可达（QEMU lab 宿主端口）
```

页面执行「新增伙伴 / 新增内容 / 提交审核」并确认列表更新、记录一次 `request_id` 用于追踪。

## 8. 可观测性

- **健康检查**：静态资源可达即健康——`GET /backoffice/`（经反代）或容器 `GET :8088/` 返回 200 且包含 `index.html`。无独立 health RPC（前端无后端运行时）。
- **前端错误上报**：网络/接口错误经 `HttpGatewayApiClient.request()` 归一为四类并展示给运营（见下「安全/错误分类」）；接入集中前端错误监控（如采集 `window.onerror` / 未捕获 Promise 拒绝）为演进项，占位行为是页面内全局错误提示 + 控制台日志。
- **关键操作埋点 / 审计入口**：每次写操作写入「最近操作日志」，字段 `action`（如 `content.publish`）、`target`、`result`、`request_id`、`timestamp`；该日志即前端侧审计入口，权威审计落在后端（gateway → 各域，事件名见 `backoffice-backend.md` §8）。
- **链路追踪**：透传并展示后端回传的 `meta.request_id` / `meta.trace_id`，便于与网关/域服务日志对账。

## 9. 安全

- **运营鉴权与角色**：见 §6.3；身份与授权裁决由 gateway/后端负责，前端不绕过。
- **最小权限与数据范围**：页面仅暴露 backoffice 契约允许的字段与动作；前端不缓存敏感凭据，不在 URL query 携带 token。
- **危险操作二次确认**：发布、下架、归档、版本回滚、审核拒绝、可见性 `restricted/unpublished` 等不可逆/高影响动作必须弹确认框并展示影响摘要（版本号、目标状态）后才调用 API。
- **错误分类（不暴露内部细节）**：`HttpGatewayApiClient.request()` 区分
  1. 网络错误 `code=-1`（fetch 抛出）；
  2. 接口未实现 `code=-2`（命中 brpc `Fail to find method`）；
  3. HTTP 非 2xx `code=<status>`；
  4. 业务错误（HTTP 200 但 `success=false`）：透传 gateway 的 `code` / `message`。

  常见业务码：`10002` 参数缺失/校验失败、`10003` 资源不存在、`10004`（或共享契约 `10007`）版本冲突——均对运营给出可读提示，不泄露后端栈信息。

## 10. 测试与上线门槛

分层策略与回归清单见 `test-plan.md`：单元/组件（页面渲染、表单校验、状态流转、操作日志）、契约调用与错误处理（integration-lite）、QEMU 部署 smoke、关键流程 E2E 上线门槛（内容发布全生命周期、审核通过/拒绝、伙伴配置、审计查询）。交付门槛见 `test-plan.md` §6。

## 11. 数据模型（不适用说明）

本服务是前端 Web，**无业务 PostgreSQL 表、无业务 proto、无领域事件产出**，故 DoD 中的 `data-model.md` / `workflow.md`（领域写链路、迁移、索引、Kafka 事件）对本服务 **不适用**。相关数据模型与状态机以 `gateway/docs/backoffice-backend.md` §4–§5 及各域 `data-model.md` 为权威；前端仅消费其对外 JSON 形态（类型定义见 `src/gateway/types.ts`，与 backoffice 契约对齐）。

## 12. 运行时与优雅退出

静态资产容器无长连接业务态；停机由编排发 SIGTERM 给静态服务器（`serve` / nginx）正常退出即可，无在途业务 RPC 需要排空。重新部署/回滚以镜像 tag 切换为准，不存在跨实例内存权威状态。

## 13. 文档索引

- `README.md`（本文件）：服务总览、依赖、SLO、构建/运行/配置、部署、可观测性、安全、测试入口。
- `detail-design.md`：页面信息架构、交互状态、字段与动作约束、内容状态机、接口契约映射。
- `test-plan.md`：分层测试、回归清单、QEMU 验收、E2E 上线门槛、交付门槛。
- `changelog.md`：契约/行为/文档变更记录。
- `../deploy/README.md`：部署与回滚 runbook。
