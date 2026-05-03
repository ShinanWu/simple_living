# kafka 兼容运行服务说明

`kafka` 运行服务当前采用 Redpanda 单节点模式（Kafka 协议兼容）。

## 启动

```bash
bash services/foundation/kafka/deploy/deploy_service.sh up
```

可选诊断（查看 compose 状态）：

```bash
docker compose -f services/foundation/kafka/deploy/docker-compose.yml ps
```

## 停止

```bash
bash services/foundation/kafka/deploy/deploy_service.sh down
bash services/foundation/kafka/deploy/deploy_service.sh down-v
```

## 默认参数

- Kafka bootstrap: `127.0.0.1:9092`
