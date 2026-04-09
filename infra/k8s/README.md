# simple_living Kubernetes Baseline (RKE2)

本目录提供 Phase2/3 初版基座清单，遵循仓库当前架构边界：`gateway` 作为对外入口，domain 服务走集群内访问。

## 目录说明

- `base/namespace.yaml`：命名空间 `simple-living`
- `base/configmap-common.yaml`：公共环境变量占位
- `base/secret-template.yaml`：密钥模板（禁止提交真实凭据）
- `base/service-*.yaml`：ClusterIP 服务
- `base/deployment-*.yaml`：Deployment 模板（镜像/探针/资源均为占位）

## RKE2 apply 顺序

建议按“先命名空间和配置，再服务，再工作负载”顺序：

```bash
kubectl apply -f infra/k8s/base/namespace.yaml
kubectl apply -f infra/k8s/base/configmap-common.yaml
kubectl apply -f infra/k8s/base/secret-template.yaml
kubectl apply -f infra/k8s/base/service-user-domain.yaml
kubectl apply -f infra/k8s/base/service-content-domain.yaml
kubectl apply -f infra/k8s/base/service-recommendation-domain.yaml
kubectl apply -f infra/k8s/base/service-affiliate-domain.yaml
kubectl apply -f infra/k8s/base/service-tracking-domain.yaml
kubectl apply -f infra/k8s/base/service-governance-domain.yaml
kubectl apply -f infra/k8s/base/service-gateway.yaml
kubectl apply -f infra/k8s/base/deployment-user-domain.yaml
kubectl apply -f infra/k8s/base/deployment-content-domain.yaml
kubectl apply -f infra/k8s/base/deployment-recommendation-domain.yaml
kubectl apply -f infra/k8s/base/deployment-affiliate-domain.yaml
kubectl apply -f infra/k8s/base/deployment-tracking-domain.yaml
kubectl apply -f infra/k8s/base/deployment-governance-domain.yaml
kubectl apply -f infra/k8s/base/deployment-gateway.yaml
```

也可在确认先创建命名空间后批量 apply：

```bash
kubectl apply -f infra/k8s/base/namespace.yaml
kubectl apply -f infra/k8s/base/
```

或使用脚本（推荐）：

```bash
bash infra/k8s/scripts/apply_base.sh
```

## 验证命令（验收）

```bash
kubectl get ns
kubectl -n simple-living get configmap common-env
kubectl -n simple-living get secret app-secrets
kubectl -n simple-living get svc
kubectl -n simple-living get deploy
kubectl -n simple-living get pods -o wide
kubectl -n simple-living rollout status deploy/user-domain
kubectl -n simple-living rollout status deploy/content-domain
kubectl -n simple-living rollout status deploy/recommendation-domain
kubectl -n simple-living rollout status deploy/affiliate-domain
kubectl -n simple-living rollout status deploy/tracking-domain
kubectl -n simple-living rollout status deploy/governance-domain
kubectl -n simple-living rollout status deploy/gateway
```

可选：检查 Service DNS 是否符合约定：

```bash
kubectl -n simple-living get svc user-domain content-domain recommendation-domain affiliate-domain tracking-domain governance-domain gateway -o wide
```

预期集群内可通过以下地址访问：

- `user-domain.simple-living.svc.cluster.local:9101`
- `content-domain.simple-living.svc.cluster.local:9102`
- `recommendation-domain.simple-living.svc.cluster.local:9103`
- `affiliate-domain.simple-living.svc.cluster.local:9104`
- `tracking-domain.simple-living.svc.cluster.local:9105`
- `governance-domain.simple-living.svc.cluster.local:9106`
- `gateway.simple-living.svc.cluster.local:8080`

## 回滚命令

按工作负载到配置的逆序删除：

```bash
kubectl delete -f infra/k8s/base/deployment-gateway.yaml
kubectl delete -f infra/k8s/base/deployment-governance-domain.yaml
kubectl delete -f infra/k8s/base/deployment-tracking-domain.yaml
kubectl delete -f infra/k8s/base/deployment-affiliate-domain.yaml
kubectl delete -f infra/k8s/base/deployment-recommendation-domain.yaml
kubectl delete -f infra/k8s/base/deployment-content-domain.yaml
kubectl delete -f infra/k8s/base/deployment-user-domain.yaml
kubectl delete -f infra/k8s/base/service-gateway.yaml
kubectl delete -f infra/k8s/base/service-governance-domain.yaml
kubectl delete -f infra/k8s/base/service-tracking-domain.yaml
kubectl delete -f infra/k8s/base/service-affiliate-domain.yaml
kubectl delete -f infra/k8s/base/service-recommendation-domain.yaml
kubectl delete -f infra/k8s/base/service-content-domain.yaml
kubectl delete -f infra/k8s/base/service-user-domain.yaml
kubectl delete -f infra/k8s/base/secret-template.yaml
kubectl delete -f infra/k8s/base/configmap-common.yaml
kubectl delete -f infra/k8s/base/namespace.yaml
```

如需快速回滚全部资源（包括命名空间内资源）：

```bash
kubectl delete ns simple-living
```

或使用脚本回滚（默认保留 namespace）：

```bash
bash infra/k8s/scripts/rollback_base.sh
```

删除 namespace：

```bash
DELETE_NAMESPACE=true bash infra/k8s/scripts/rollback_base.sh
```

## 安全说明

- `secret-template.yaml` 仅用于字段占位和结构约束。
- 真实密钥请通过外部密钥管理（如 Sealed Secrets / External Secrets / CI 注入）生成，避免写入 git。
- 所有镜像均为占位地址，请在部署前替换为真实仓库与 tag。

## DNS / 端口验收清单（全量 6 个 domain）

建议在 `simple-living` 命名空间内启动一个临时调试 Pod 做 DNS 和连通性校验：

```bash
kubectl -n simple-living run dnscheck --image=busybox:1.36 --restart=Never -- sleep 3600
kubectl -n simple-living exec -it dnscheck -- nslookup user-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup content-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup recommendation-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup affiliate-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup tracking-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup governance-domain.simple-living.svc.cluster.local
kubectl -n simple-living exec -it dnscheck -- nslookup gateway.simple-living.svc.cluster.local
kubectl -n simple-living delete pod dnscheck
```

端口约定（对齐 `docs/engineering-conventions.md`）：

- `user-domain`: `9101`
- `content-domain`: `9102`
- `recommendation-domain`: `9103`
- `affiliate-domain`: `9104`
- `tracking-domain`: `9105`
- `governance-domain`: `9106`
- `gateway`: `8080`

也可先跑就绪检查脚本：

```bash
bash infra/k8s/scripts/check_ready.sh simple-living
```
