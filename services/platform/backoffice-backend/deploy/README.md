# backoffice-backend — 部署

## 1. 产物

- Bazel：`//services/platform/backoffice-backend:backoffice_backend_server`
- 配置：`-pg_conninfo`、`-export_dir`、`-secret_backend_uri`

## 2. 快照目录与消费者

**生产 / 同机联调**：与 `recommendation-server` **必须**同 Pod/VM，共享同一物理目录：

```text
/shared/exports/          # hostPath 或 PVC
  active/
  staging/
```

**local-qemu（多来宾）**：各来宾磁盘独立，路径虽同为 `/var/lib/simple-living/exports`，内容不同步。部署 backoffice 时可启用 **来宾侧 inotify fan-out**（见 §6）：监听 `export_dir/active/`，变更后 rsync 到 recommendation / tracking 来宾。

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
| `setup_snapshot_fanout_remote.sh` | lab：`ENABLE_SNAPSHOT_FANOUT=1` 时安装 inotify fan-out systemd |
| `start_nodes.sh` / `check_nodes.sh` / `stop_nodes.sh` | QEMU 节点生命周期 |

`recommendation-server` 的 `start_nodes.sh` 委托本目录脚本（同机联调）。

## 6. 快照 fan-out（local-qemu）

`nodes.env` 中 `ENABLE_SNAPSHOT_FANOUT=1` 时，`deploy_service.sh` 在 backoffice 来宾安装 `simple-living-snapshot-fanout.service`：

- 监听 `${EXPORT_DIR}/active/`（`inotifywait`）
- 防抖后执行 `snapshot_fanout_push.sh`，经 slirp 网关 `LAB_QEMU_GATEWAY_HOST` 与 Mac 转发的 SSH 端口推到消费者来宾
- 依赖：`inotify-tools`、`rsync`、来宾间 SSH（与部署用同一私钥，安装到 `~/.ssh/simple_living_fanout`）

同机共享 `EXPORT_DIR` 时保持 `ENABLE_SNAPSHOT_FANOUT=0`（默认）。
