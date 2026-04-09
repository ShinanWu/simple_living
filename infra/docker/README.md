# C++ 服务容器化模板

本目录提供 7 个服务（`gateway` + 6 个 domain）的统一镜像构建与推送模板（Docker/Podman）。

## 文件说明

- `Dockerfile.service`：通用运行时镜像模板（`debian` slim），将服务二进制复制为 `/app/server` 并默认执行。
- `build_images.sh`：循环构建 7 个服务镜像；支持 `REGISTRY`、`IMAGE_TAG`；从 `bazel-bin` 复制二进制到临时目录后执行 `docker build`。
- `push_images.sh`：按同一服务列表执行 `docker push`。

## 前置条件

1. 在仓库根目录完成服务二进制构建（必须先有 `bazel-bin` 输出）：

```bash
bazel build \
  //services/gateway:gateway_edge_server \
  //services/user-domain:user_domain_server \
  //services/content-domain:content_domain_server \
  //services/recommendation-domain:recommendation_domain_server \
  //services/affiliate-domain:affiliate_domain_server \
  //services/tracking-domain:tracking_domain_server \
  //services/governance-domain:governance_domain_server
```

2. 已安装并可使用 Docker 或 Podman。

## 示例命令

在仓库根目录执行：

```bash
# 仅本地构建（无仓库前缀）
IMAGE_TAG=v0.1.0 ./infra/docker/build_images.sh

# 构建并带仓库前缀
REGISTRY=registry.example.com/simple-living IMAGE_TAG=v0.1.0 ./infra/docker/build_images.sh

# 推送（push 必须指定 REGISTRY）
REGISTRY=registry.example.com/simple-living IMAGE_TAG=v0.1.0 ./infra/docker/push_images.sh

# 强制使用 podman
CONTAINER_CLI=podman IMAGE_TAG=v0.1.0 ./infra/docker/build_images.sh
```

## 服务与二进制映射

- `gateway` -> `gateway_edge_server`
- `user-domain` -> `user_domain_server`
- `content-domain` -> `content_domain_server`
- `recommendation-domain` -> `recommendation_domain_server`
- `affiliate-domain` -> `affiliate_domain_server`
- `tracking-domain` -> `tracking_domain_server`
- `governance-domain` -> `governance_domain_server`
