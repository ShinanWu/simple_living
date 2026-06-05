# environments（运行环境定义）

本目录描述**如何跑起来**，不承载业务服务代码。业务实现仍在 `services/`。

| 子目录 | 用途 |
|--------|------|
| [`local-qemu/`](./local-qemu/README.md) | Mac 本地 **QEMU 多节点联调**：`nodes.env`、VM 磁盘、`vms/` 运行时 |
| [`kubernetes/`](./kubernetes/README.md) | **K8s 集群公共基座**：namespace、configmap、secret 模板 |

## 与服务 deploy 的关系

- 各服务部署入口：`services/<service>/deploy/deploy_service.sh`（C++ 服务共用 [`tools/deploy_cpp_service.sh`](../tools/deploy_cpp_service.sh)）
- 默认读取：`environments/local-qemu/nodes.env`（模板见 `nodes.example.env`）
- 数据库 / Redis / Kafka 运行实例：`services/foundation/`（连接参数来自 `nodes.env`）

## 快速开始（本地 QEMU）

```bash
cp environments/local-qemu/nodes.example.env environments/local-qemu/nodes.env
# 编辑 nodes.env 后，按服务 README 逐个 deploy
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/gateway/deploy/deploy_service.sh
```
