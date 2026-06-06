# PostgreSQL 容器部署

仅管理 **PostgreSQL 容器**（`simple-living-postgres`）。

QEMU 节点与三组件一键部署见上级目录：

- `services/foundation/deploy/start_nodes.sh`
- `services/foundation/deploy/deploy_service.sh`

```bash
bash services/foundation/postgres/deploy/deploy_service.sh up
bash services/foundation/postgres/deploy/deploy_service.sh health
bash services/foundation/postgres/deploy/deploy_service.sh down
```
