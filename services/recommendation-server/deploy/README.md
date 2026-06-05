# recommendation-server 独立部署说明（QEMU）

## 1. 同节点约束（v1）

**必须与 `platform/backoffice-backend` 同 VM/Pod**，共享导出目录：

```text
/var/lib/simple-living/exports/   # 与 backoffice -export_dir 一致
```

先启动 `backoffice-backend` 并完成至少一次导出，再启动本服务。

## 2. 执行入口

```bash
bash services/recommendation-server/deploy/start_nodes.sh
bash services/recommendation-server/deploy/check_nodes.sh
bash services/recommendation-server/deploy/deploy_service.sh
```

## 3. 配置

| 项 | 说明 |
|----|------|
| `-port` | `9103` |
| `-snapshot_dir` | 与 backoffice `export_dir` 相同 |
| `-user_server_addr` | `brpc://127.0.0.1:9101` |

## 4. 验收

- readiness 依赖有效 manifest + mmap 校验通过
- `QueryRecommendations` / `BatchGetGuideCards` 对 fixture 或发布后 snapshot 返回可见卡片

## 5. 回滚

镜像回滚同原流程；读面回滚可仅将 backoffice 导出 `active_version` 指回上一版，本服务自动热切换。
