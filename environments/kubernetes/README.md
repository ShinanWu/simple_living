# environments/kubernetes（集群公共基座）

`environments/kubernetes` 只保留**不承接用户流量**的集群公共资源：

- `namespace.yaml`
- `configmap-common.yaml`
- `secret-template.yaml`

业务服务（`gateway` / 各 `*-server` 与 `backoffice-backend`）的 `Deployment` 与 `Service` 已下沉到各自目录：

- `services/<service>/deploy/k8s/`

## 使用方式

### 1) 应用公共基座

```bash
bash environments/kubernetes/scripts/apply_base.sh
```

### 2) 部署某个服务（示例）

```bash
kubectl apply -f services/gateway/deploy/k8s/service-gateway.yaml
kubectl apply -f services/gateway/deploy/k8s/deployment-gateway.yaml
```

### 3) 就绪检查

检查命名空间所有 deployment：

```bash
bash environments/kubernetes/scripts/check_ready.sh simple-living
```

只检查指定 deployment：

```bash
DEPLOYMENTS="gateway user-server" bash environments/kubernetes/scripts/check_ready.sh simple-living
```

### 4) 回滚公共基座

```bash
bash environments/kubernetes/scripts/rollback_base.sh
```

如需删除命名空间：

```bash
DELETE_NAMESPACE=true bash environments/kubernetes/scripts/rollback_base.sh
```
