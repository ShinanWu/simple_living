# QEMU Lab（单服务部署模型）

本目录只提供公共基础设施原语。  
业务服务部署入口在各自目录：`services/<service>/deploy/deploy_service.sh` 与 `services/platform/backoffice-web/deploy/deploy_service.sh`。

## 脚本清单

- 无统一服务编排脚本。
- 服务部署、镜像构建、镜像分发、容器启动均在服务目录内执行。
- 节点生命周期入口也下沉到服务目录：
  - `services/<service>/deploy/start_nodes.sh`
  - `services/<service>/deploy/check_nodes.sh`
  - `services/<service>/deploy/stop_nodes.sh`
  - `services/platform/backoffice-web/deploy/start_nodes.sh`
  - `services/platform/backoffice-web/deploy/check_nodes.sh`
  - `services/platform/backoffice-web/deploy/stop_nodes.sh`

## 强约束

- 不提供批量全服务编排脚本。
- 不维护服务清单中心脚本。
- 每个服务在自身 `deploy/deploy_service.sh` 中定义：
  - 构建目标（Bazel target 或 Docker build context）
  - 镜像名
  - 节点与端口
  - 运行参数

## 使用流程

```bash
cp infra/lab/nodes.example.env infra/lab/nodes.env
bash services/gateway/deploy/start_nodes.sh
bash services/gateway/deploy/check_nodes.sh
bash services/gateway/deploy/deploy_service.sh
```

## Bazel 版本

- `.bazelversion` 是唯一版本源。
- 优先 `bazelisk`，回退 `bazel`。
