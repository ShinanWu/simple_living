# user-server 独立部署说明（QEMU）

## 1. 执行入口

```bash
bash services/user-server/deploy/start_nodes.sh
bash services/user-server/deploy/check_nodes.sh
bash services/user-server/deploy/deploy_service.sh
```

仅处理 `user-server`，不会构建或发布其他服务。

## 1.1 配置注入（机密不入库）

部署时通过环境变量 / 部署密钥注入连接串与机密（见 [README.md](../README.md)）：

```bash
export PG_PWD=...                       # 数据库口令
export USER_TOKEN_HMAC_KEY=...          # token 签名/指纹 HMAC 密钥
export USER_PII_ENC_KEY=...             # 手机号等 PII 字段加密键（或 KMS key id）
export USER_PHONE_HASH_SALT=...         # 手机号 HMAC 查找盐
# 启动参数：-port=9101 -pg_conninfo="host=... dbname=user user=user password=$PG_PWD" \
#           -redis_addr=... -kafka_brokers=...
```

**数据库迁移**：发布前对目标 PostgreSQL 执行 `services/foundation/migrations/NNNN_user_*.sql`（幂等，记录于 `schema_migrations`）。迁移须向后兼容（先加列/表，再发新版本代码），便于回滚。

## 2. 回滚

1. **应用回滚**：重新部署上一个已知良好镜像标签：

```bash
IMAGE_TAG=v2026.04.27 bash services/user-server/deploy/deploy_service.sh
```

2. **验证回滚**：执行 §3 验收命令确认 `/healthz` 200 且关键 RPC 可用。
3. **数据库**：本域迁移要求向后兼容，应用回滚通常**无需**回退 schema；若某迁移确含破坏性变更，则该迁移必须随附反向脚本并在 Git 提交/PR 说明，回滚时按需执行。
4. 如需停服：

```bash
bash services/user-server/deploy/stop_nodes.sh
```

K8s 部署回滚见 §5。

## 3. 验收（健康检查与冒烟）

```bash
# 1) 健康检查（就绪含 PostgreSQL 连通性）
ssh -p 2202 ubuntu@127.0.0.1 "curl -fsS http://127.0.0.1:9101/healthz"

# 2) 进程/端口存活
ssh -p 2202 ubuntu@127.0.0.1 "ss -ltnp | grep 9101"

# 3) 关键链路冒烟：签发→内省→撤销（经联调脚本或 brpc 客户端，断言 valid 翻转）
```

验收通过判据：`/healthz` 返回 200；`9101` 端口监听；登录签发→内省→撤销闭环符合 [api.md](../api.md) 语义。

## 4. 故障排查

| 现象 | 可能原因 | 排查 |
|------|----------|------|
| `/healthz` 非 200 | PostgreSQL 不可达 / 连接池耗尽 | 检查 `-pg_conninfo`、网络与 `pg_pool_size`；看启动日志 PG 连接错误 |
| 启动即退出（非零码） | 配置缺失（`-pg_conninfo`/密钥）或迁移失败 | 核对必填 flag 与机密环境变量；确认 `schema_migrations` 与迁移文件 |
| 内省全部 `valid=false` | token HMAC 密钥不一致 / 会话被全撤 | 核对 `USER_TOKEN_HMAC_KEY`；查 `user_session.revoked_at` |
| 事件不投递 / 积压 | `-kafka_brokers` 未配或 relay 停 | 查 `user_outbox` 未投递积压指标与 relay 进程 |
| OTP 总失败 | 盐/时钟漂移 / OTP 过期 | 核对 `USER_PHONE_HASH_SALT`、节点时间、`-otp_ttl_ms` |

## 4.1 日志位置

- 容器/编排：标准输出（`kubectl logs deploy/user-server` 或 `journalctl -u user-server`）。
- QEMU 节点：服务日志默认随 `deploy_service.sh` 落到运行用户目录下的服务日志文件（结构化 JSON，含 `request_id`/`trace_id`）。
- 日志**不含**完整 token / 手机号 / OTP 明文。

## 4.2 优雅退出

- 停服走 `SIGTERM` → 停收新请求 → 等待在途 RPC → 关 outbox relay/Redis/PostgreSQL（先停对外、再关依赖），正常退出码 `0`。编排默认发 `SIGTERM`，与本地 `Ctrl+C` 行为一致。

## 5. 跨服务协作原则

- 仅通过契约文档协作：`services/gateway/api.md` 与各服务 `api.md`。
- 不直接依赖其他服务源码与内部实现细节。

## 6. K8s（服务内部署入口）

```bash
bash services/user-server/deploy/k8s/apply.sh
bash services/user-server/deploy/k8s/rollback.sh
```
