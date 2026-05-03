# postgres 运行服务说明

`postgres` 作为运行时基础服务，为业务域提供关系型存储能力。

## 启动

```bash
bash services/foundation/postgres/deploy/deploy_service.sh up
```

可选诊断（查看 compose 状态）：

```bash
docker compose -f services/foundation/postgres/deploy/docker-compose.yml ps
```

## 停止

```bash
bash services/foundation/postgres/deploy/deploy_service.sh down
bash services/foundation/postgres/deploy/deploy_service.sh down-v
```

## 默认参数

- Port: `5432`
- Database: `simple_living`
- User/Password: `simple` / `simple`
