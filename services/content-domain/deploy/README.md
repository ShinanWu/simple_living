# content-domain 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/content-domain/deploy/start_nodes.sh
bash services/content-domain/deploy/check_nodes.sh
bash services/content-domain/deploy/deploy_service.sh
```

仅处理 `content-domain`，不会构建或发布其他服务。

## 2. 回滚

```bash
IMAGE_TAG=v2026.04.27 bash services/content-domain/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/content-domain/deploy/stop_nodes.sh
```

## 3. 验收

```bash
ssh -p 2203 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:9102/healthz"
```

## 4. 跨服务协作原则

- 仅通过契约文档协作：`docs/contracts/` 与 `services/*/docs/api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 5. K8s（服务内部署入口）

```bash
bash services/content-domain/deploy/k8s/apply.sh
bash services/content-domain/deploy/k8s/rollback.sh
```
