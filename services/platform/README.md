# platform 服务组说明

`services/platform/` 用于承载平台侧（非终端 C 端）服务实现。

当前包含：

- `backoffice-web/`：运营管理后台 Web（affiliate/content/governance 运营动作入口）

边界约束：

- 不放终端用户 App Web（C 端）页面实现。
- 仅承载平台侧管理能力与对应部署入口。
