# QEMU Lab Multi-Node Deployment

This directory provides automation scripts for distributed deployment in a QEMU lab.
Each service can run on a different Linux QEMU node, and each node runs containers with Docker or Podman.
The public remote host is only for intranet tunneling (frps), not for build.

## Directory Layout

- `nodes.example.env`: environment template (SSH and node IP mapping).
- `scripts/check_nodes.sh`: verify node prerequisites over SSH.
- `scripts/start_qemu_nodes.sh`: start local multi-QEMU ARM Linux nodes.
- `scripts/stop_qemu_nodes.sh`: stop local QEMU nodes.
- `scripts/build_on_qemu.sh`: build binaries and images on local QEMU build node.
- `scripts/distribute_images.sh`: stream images from build node to service nodes.
- `scripts/deploy_by_service.sh`: deploy each service to its configured node.
- `scripts/stop_all.sh`: stop and remove deployed service containers.
- `scripts/run_full_pipeline.sh`: one-shot pipeline for build, distribute, deploy, and checks.

## 1) Prepare Environment File

1. Copy template:

```bash
cp infra/qemu-lab/nodes.example.env infra/qemu-lab/nodes.env
```

2. Edit `infra/qemu-lab/nodes.env`:
   - Fill `SSH_USER`, `SSH_KEY_PATH`, `SSH_PORT`.
   - If all nodes share `127.0.0.1`, use per-node SSH port overrides (`NODE_SSH_PORT_*`).
   - Fill node IPs:
     - `NODE_IP_BUILD`
     - `NODE_IP_GATEWAY`
     - `NODE_IP_USER`
     - `NODE_IP_CONTENT`
     - `NODE_IP_RECOMMENDATION`
     - `NODE_IP_AFFILIATE`
     - `NODE_IP_TRACKING`
     - `NODE_IP_GOVERNANCE`
     - `NODE_IP_NGINX`
     - `NODE_IP_FRPC`
   - Optional build path:
     - `QEMU_BUILD_WORKDIR` (default `/root/simple_living`)
   - Optional image settings:
     - `REGISTRY` (for example `registry.example.com/team`)
     - `IMAGE_TAG` (default `latest`)

## 2) Execution Flow

Run from repository root:

### Step A: start_qemu_nodes (if nodes are not running)

```bash
bash infra/qemu-lab/scripts/start_qemu_nodes.sh
```

This will create local QEMU nodes and map SSH ports `2201~2210` to localhost.

### Step B: check_nodes

```bash
bash infra/qemu-lab/scripts/check_nodes.sh
```

This checks each configured node via SSH and validates `docker`, `systemctl`, and `curl`.
Any failed node is summarized at the end, and the script exits non-zero.

### Step C: build_on_qemu

```bash
bash infra/qemu-lab/scripts/build_on_qemu.sh
```

This runs Bazel build and image build on `NODE_IP_BUILD`.

### Step D: distribute_images

```bash
bash infra/qemu-lab/scripts/distribute_images.sh
```

This streams local images from build node to each target node when registry is not used.

### Step E: deploy_by_service

```bash
bash infra/qemu-lab/scripts/deploy_by_service.sh
```

For each service, the script runs on the mapped node:
- optional pull from registry (if `REGISTRY` provided)
- replace old container with `<docker|podman> rm -f`
- start container by `<docker|podman> run -d --restart unless-stopped`

Container naming rule:
- `simple-living-gateway`
- `simple-living-user-domain`
- `simple-living-content-domain`
- `simple-living-recommendation-domain`
- `simple-living-affiliate-domain`
- `simple-living-tracking-domain`
- `simple-living-governance-domain`

Port mapping rule:
- `gateway`: `8080:8080`
- `user-domain`: `9101:9101`
- `content-domain`: `9102:9102`
- `recommendation-domain`: `9103:9103`
- `affiliate-domain`: `9104:9104`
- `tracking-domain`: `9105:9105`
- `governance-domain`: `9106:9106`

Image rule:
- with registry: `${REGISTRY}/<service>:${IMAGE_TAG}`
- without registry: `<service>:${IMAGE_TAG}`

### Step F: ingress check

After service deployment, verify ingress/frpc related nodes manually (example):

```bash
ssh -i ~/.ssh/your_private_key your_ssh_user@<nginx_node_ip> "sudo systemctl status nginx --no-pager"
ssh -i ~/.ssh/your_private_key your_ssh_user@<frpc_node_ip> "sudo systemctl status frpc --no-pager"
```

### Step G: gateway smoke

Run a basic gateway smoke test (replace endpoint as needed):

```bash
curl -fsS "http://<gateway_node_ip>:8080/healthz"
```

## 3) Stop All Services

```bash
bash infra/qemu-lab/scripts/stop_all.sh
```

This stops and removes all service containers (`simple-living-<service>`) on their mapped nodes.

Stop QEMU nodes:

```bash
bash infra/qemu-lab/scripts/stop_qemu_nodes.sh
```

## 4) One-shot run

```bash
BASE_URL="https://<your-domain>" bash infra/qemu-lab/scripts/run_full_pipeline.sh
```

Default `infra/qemu-lab/nodes.env` in this repo is prefilled for local multi-QEMU via `127.0.0.1 + different ssh ports`.

## Known Limitations

- Scripts assume QEMU nodes have Docker or Podman and the SSH user can run the container CLI.
- Scripts do not provision nodes, install dependencies, or configure firewall/security groups.
- `deploy_by_service.sh` only deploys the fixed service list and fixed ports in this README.
- Ingress/frpc deployment is not automated here; only service containers are automated.
- Health check path is environment-specific and may need customization.
