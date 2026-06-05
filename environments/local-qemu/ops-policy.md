# QEMU Lab Ops Policy（强约束）

## 目标

定义“服务自治部署”边界：每个服务独立构建/发布，不允许全服务编排入口。

## 适用范围

- `environments/local-qemu/`
- `services/*/deploy/`
- `services/platform/backoffice-web/deploy/`

## 强约束

1. 仅允许单服务部署入口：`services/<service>/deploy/deploy_service.sh` 或 `services/platform/backoffice-web/deploy/deploy_service.sh`。
2. 禁止新增“批量处理所有服务”的脚本与命令。
3. 节点生命周期入口也必须服务内自治：`start_nodes.sh / check_nodes.sh / stop_nodes.sh`。
4. 服务间协作仅通过契约文档，不通过部署脚本相互耦合。

## 推荐流程

```bash
bash services/<service>/deploy/start_nodes.sh
bash services/<service>/deploy/check_nodes.sh
bash services/<service>/deploy/deploy_service.sh
```

## 版本策略

- `.bazelversion` 为唯一 Bazel 版本来源。
- build 节点优先 `bazelisk`。
