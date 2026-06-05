# tools

仓库级共享脚本。各服务/环境另有专用脚本，见下表。

## 本目录

| 脚本 | 用途 |
|------|------|
| [`deploy_cpp_service.sh`](./deploy_cpp_service.sh) | QEMU 上构建 Bazel C++ 服务镜像并部署（由各 `services/*/deploy/deploy_service.sh` 调用） |
| [`regenerate_brpc_stubs.sh`](./regenerate_brpc_stubs.sh) | 本地重新生成 brpc stub（开发辅助） |
| [`generate_brpc_stub_server.py`](./generate_brpc_stub_server.py) | brpc stub 生成器实现 |

## 脚本地图（其他位置）

| 位置 | 示例 |
|------|------|
| `services/foundation/scripts/` | `run_migrations.sh`、`health_check.sh`、`backup.sh`、`restore.sh` |
| `services/proxy/src/scripts/` | `check_ingress.sh`（配合 `proxy` manual smoke test） |
| `environments/kubernetes/scripts/` | `apply_base.sh` |
| `environments/local-qemu/` | `nodes.env`、`vms/`（QEMU 联调） |
| `client/tests/` | `gateway_api_smoke.py`（C 端 gateway 烟雾） |
| `client/wechat-miniprogram/scripts/` | `run-tests.ts` |
| `client/ios/scripts/` | `fix_local_spm_package_link.py` |
| `services/platform/backoffice-web/deploy/` | 运营 Web QEMU/Docker 部署（与 `client/` 无代码关联） |
