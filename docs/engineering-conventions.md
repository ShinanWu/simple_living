# 工程约定（跨服务公共）

本文档沉淀与具体服务目录无关、在本仓库内实现、联调与测试时应共同遵守的约定：Bazel、运行时端口、跨服务依赖、`gateway` 协议分层、测试与 CI。

各服务的实现与交付说明（本包 Bazel 目标、端口、`bazel run` 示例、合并前检查清单）在 **`services/<service>/docs/development.md`**；阅读顺序：本服务 `docs/` 下业务文档 → `development.md` → 本文中与本服务相关章节。

---

## 1. 通用规则

| 规则 | 说明 |
|------|------|
| 按服务边界修改代码 | 默认只修改 `services/<service>/` 下 `src/`、`proto/`、`BUILD.bazel`、`tests/`、`docs/` |
| 不 fork 他域 proto | 调用方仅在 Bazel `deps` 中依赖提供方 `proto_library` / `cc_proto_library` |
| 公共 JSON 语义 | 变更多端或跨域字段含义时，先改 `docs/contracts/`，再改 `services/gateway/docs/` 与相关域 |
| 客户端入口 | 终端 **只** 对接 `gateway` 的 **HTTPS+JSON**；不得假设客户端直连业务域 brpc |

### 1.1 契约真源

- **对外 JSON**：以 `docs/contracts/` 与 `services/gateway/docs/api.md` 为准；映射层与实现冲突时改实现。
- **内部 RPC 有线格式**（字段号、枚举值、`oneof`）：以提供方 **`*.proto` 为准**；`api.md` 与 proto 不一致时，修正 `api.md` 并与 proto 同变更交付。

### 1.2 何时必须协调其它服务

| 变更类型 | 必须同步 |
|----------|----------|
| 本域 proto 破坏性变更 | 所有 Bazel 依赖本域 `cc_proto` 的包、`gateway`、本域 `changelog.md` |
| 公共 JSON / 错误码语义 | `docs/contracts/`、`services/gateway/docs/`、相关域 `docs/` |
| 新增跨域运行时依赖 | [`docs/architecture/README.md`](./architecture/README.md) 依赖说明、双方 `docs/`、调用方 `BUILD.bazel` |

### 1.3 禁止事项

- 在 `docs/contracts/` 放入仅单域使用的私有 API。
- 未在 `gateway` 与 contracts 中定义的对外 JSON 字段，不得从内部 proto 直接暴露。
- 在消费方目录复制提供方 `proto` 并自行修改。

---

## 2. Bazel 与仓库构建

| 项 | 说明 |
|----|------|
| Bazel | Bazelisk；版本见仓库根 `.bazelversion` |
| 模块 | 根 `MODULE.bazel`：`brpc`、`protobuf`、`rules_cc`、`rules_proto` 等 |
| 共享编译选项 | `//build:brpc_copts.bzl` 中 `BRPC_EXAMPLE_COPTS`；各 `cc_binary` / `cc_test` 应 `load` 后使用，避免 FLAGS 分裂 |

仓库须存在：

```text
build/
├── BUILD.bazel
└── brpc_copts.bzl
```

`build/brpc_copts.bzl` 推荐内容以仓库内实际文件为准；新增目标用 `copts = BRPC_EXAMPLE_COPTS + [...]` 追加，勿复制整表。

### 2.1 标准目标形态（各包内命名以 `BUILD.bazel` 为准）

| 目标 | 典型规则 |
|------|----------|
| Proto | `:*_proto`（如 `:user_domain_proto`） |
| C++ proto | `:*_cc_proto` |
| 服务端二进制 | `:*_server` 或 `gateway_edge_server` |

### 2.2 跨服务 `proto` / `cc_proto` 依赖方向

调用方 **只** 依赖提供方暴露的 Bazel 目标，**不** 拷贝 `.proto`。

| 消费方 | 可提供方（目标名以各包为准） |
|--------|------------------------------|
| `gateway` | 各业务域已导出的 `proto_library` / `cc_proto_library` |
| `recommendation-domain` | `user-domain`、`content-domain`；可选 `governance-domain` |
| `tracking-domain` | `affiliate-domain` |
| `content-domain` | `governance-domain`；可选 `affiliate-domain` |
| `governance-domain` | `content-domain`；可选 `user-domain` |
| `user-domain`、`affiliate-domain` | 默认无业务域 proto 依赖 |

业务含义见 [`docs/architecture/README.md`](./architecture/README.md) 依赖图。

**BUILD 示例**：

```python
# 消费方 proto_library
deps = ["//services/user-domain:user_domain_proto"]

# 消费方 cc_binary（RPC 客户端）
deps = [
    ":my_service_cc_proto",
    "//services/user-domain:user_domain_cc_proto",
    "@brpc//:brpc",
]
```

各服务 `proto_library` 使用 `strip_import_prefix = "proto"` 时，同包 `import` 用文件名；跨包以 `bazel build` 能解析为准。

### 2.3 常用命令

```bash
bazel build //services/...
bazel build //services/user-domain:all
bazel test //services/...
```

CI 见 `.github/workflows/bazel.yml`。

---

## 3. Gateway：对外 HTTPS+JSON 与内部 brpc

- **终端 → `gateway`**：**HTTPS + JSON**；路径与体字段见 `services/gateway/docs/api.md` 与 **`docs/contracts/`**（`snake_case`）。
- **`gateway` → 业务域**：**brpc + proto**。
- **`gateway/proto` 中的 Edge service**：与 HTTP 路由对应的 **逻辑处理边界**（桩与类型复用），**不是**客户端直连的 wire API。
- **交付**：`gateway` 二进制须含 **HTTP 接入层**（TLS/路由/JSON 信封/下游 brpc）；若仅有 brpc Service 脚手架，须在实现中补齐 HTTP 并在 `services/gateway/docs/changelog.md` 说明。

实现分层示意：

```text
HTTP 接入层 → brpc Channel → 各业务域 brpc Server
```

错误信封与字段语义见 [`docs/contracts/common-response.md`](./contracts/common-response.md)。metadata 与阶段划分见 `services/gateway/docs/workflow.md`。

---

## 4. 运行时与本地联调（默认约定）

监听地址默认 **`0.0.0.0`**；端口与标志为 **开发约定**，实现用 **gflags**（或等价）覆盖。

| 服务 | 默认端口 | 协议 | Bazel 二进制目标（以 BUILD 为准） |
|------|----------|------|-----------------------------------|
| `gateway` | `8080` | HTTPS+JSON（对外）；本地可用 HTTP 仅作开发 | `//services/gateway:gateway_edge_server` |
| `user-domain` | `9101` | brpc | `//services/user-domain:user_domain_server` |
| `content-domain` | `9102` | brpc | `//services/content-domain:content_domain_server` |
| `recommendation-domain` | `9103` | brpc | `//services/recommendation-domain:recommendation_domain_server` |
| `affiliate-domain` | `9104` | brpc | `//services/affiliate-domain:affiliate_domain_server` |
| `tracking-domain` | `9105` | brpc | `//services/tracking-domain:tracking_domain_server` |
| `governance-domain` | `9106` | brpc | `//services/governance-domain:governance_domain_server` |

**调用方下游地址 flags**（默认值建议 `brpc://127.0.0.1:<上表端口>`）：

| 标志 | 下游 |
|------|------|
| `-user_domain_addr` | user-domain |
| `-content_domain_addr` | content-domain |
| `-recommendation_domain_addr` | recommendation-domain |
| `-affiliate_domain_addr` | affiliate-domain |
| `-tracking_domain_addr` | tracking-domain |
| `-governance_domain_addr` | governance-domain |

另：`-port`、可选 `-listen_addr`。

**启动顺序（最小联调）**：先启无依赖或少依赖域（如 user、content、affiliate）→ 再启 recommendation、tracking、governance → 最后 **gateway**。

无 DB/缓存时允许 **dev stub 模式**（内存桩），须在对应 `api.md` 说明健康检查与降级语义。

---

## 5. 测试策略

| 层级 | 典型位置 |
|------|----------|
| 单元测试 | `services/<service>/tests/*_test.cc` 或 `src/*_test.cc` |
| 服务级集成 | `services/<service>/tests/integration_*` |
| 契约 / JSON 回归 | `services/gateway/tests/` 等 |

- 使用 `cc_test`，`deps` 含本服务 `cc_proto_library`、`@brpc//:brpc`、统一版本的 gtest（由首个接入测试的变更声明 Bazel 依赖）。
- 断言字段名与错误码须对齐 `api.md` 与 [`docs/contracts/error-codes.md`](./contracts/error-codes.md)。
- 示例 ID 与时间格式见 [`docs/examples/README.md`](./examples/README.md)。

**最低期望（可迭代）**：各域至少有 `HealthCheck`（若契约定义）或一条核心 RPC 的 happy path；`gateway` 至少一条 HTTP 成功 + 一条典型错误码，且顶层信封 `code` 与 `success` 一致。

---

## 6. 与公共契约目录的关系

- **JSON 字段、信封、错误码、鉴权、分页、导购卡/推荐/跳转语义**：以 **`docs/contracts/`** 为准（见 [`docs/contracts/README.md`](./contracts/README.md)）。
- **本文**：不定义业务 JSON schema，只约束工程与协作方式。
