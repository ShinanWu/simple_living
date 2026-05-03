# redis 运行服务说明

`redis` 作为运行时基础服务，为缓存与轻量状态能力提供支撑。

## 启动

```bash
bash services/foundation/redis/deploy/deploy_service.sh up
```

可选诊断（查看 compose 状态）：

```bash
docker compose -f services/foundation/redis/deploy/docker-compose.yml ps
```

## 停止

```bash
bash services/foundation/redis/deploy/deploy_service.sh down
bash services/foundation/redis/deploy/deploy_service.sh down-v
```

## 默认参数

- Port: `6379`
- AOF: enabled
