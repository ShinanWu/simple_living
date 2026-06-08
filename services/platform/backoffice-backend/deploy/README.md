# backoffice-backend — 部署

## 1. 产物

- Bazel：`//services/platform/backoffice-backend:backoffice_backend_server`
- 配置：`-pg_conninfo`、`-export_dir`、`-secret_backend_uri`

## 2. 同节点编排（硬约束）

与 `recommendation-server` **必须**同 Pod/VM：

```text
/shared/exports/          # hostPath 或 PVC
  active/
  staging/
```

启动顺序：

1. PostgreSQL / Redis / Kafka（foundation）
2. **backoffice-backend**（写权威 + 导出）
3. **recommendation-server**（mmap 读）
4. tracking-server、user-server、gateway

## 3. 健康检查

```bash
# brpc health + 导出滞后
curl -sf http://127.0.0.1:9110/health  # 以实现为准
```

验收：完成一次内容发布后，`recommendation-server` manifest `active_version` 递增。

## 4. 回滚

- 应用回滚：部署上一镜像 tag
- 数据回滚：PostgreSQL 迁移逆操作 + 重新触发全量导出
- 导出回滚：将 manifest `active_version` 指回上一稳定 `export_job` 记录（运维脚本，实现阶段补充）

## 5. 部署脚本

| 脚本 | 用途 |
|------|------|
| `deploy_service.sh` | 构建镜像并部署（调用 `tools/deploy_cpp_service.sh`） |
| `start_nodes.sh` / `check_nodes.sh` / `stop_nodes.sh` | QEMU 节点生命周期 |

`recommendation-server` 的 `start_nodes.sh` 委托本目录脚本（同机联调）。
