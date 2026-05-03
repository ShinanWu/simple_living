# backoffice-web 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/platform/backoffice-web/deploy/start_nodes.sh
bash services/platform/backoffice-web/deploy/check_nodes.sh
bash services/platform/backoffice-web/deploy/deploy_service.sh
```

仅处理 `backoffice-web`，不会构建或发布其他服务。

## 2. 回滚

```bash
IMAGE_TAG=v2026.04.27 bash services/platform/backoffice-web/deploy/deploy_service.sh
```

停止当前服务相关节点：

```bash
bash services/platform/backoffice-web/deploy/stop_nodes.sh
```

## 3. 验收

```bash
curl -fsS "http://127.0.0.1:18081" | sed -n '1,10p'
```

## 4. 跨服务协作原则

- 仅通过契约文档协作：`docs/contracts/` 与 `services/*/docs/api.md`。
- 不直接依赖其他服务源码与内部实现细节。
