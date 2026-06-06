# Kafka 容器部署

仅管理 **Kafka 容器**（`simple-living-kafka`）。

QEMU 节点与三组件一键部署见 `services/foundation/deploy/`。

```bash
bash services/foundation/kafka/deploy/deploy_service.sh up
bash services/foundation/kafka/deploy/deploy_service.sh health
bash services/foundation/kafka/deploy/deploy_service.sh down
bash services/foundation/kafka/deploy/deploy_service.sh relay   # outbox relay
```
