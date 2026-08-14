# Tests

该目录用于可重复运行的项目脚本和组件边界测试。

Innogpu 只验证设备钩子与 dotconfig 显示引擎的接入边界：

```sh
tests/xdisplay/run-install-tests.sh
```

该测试只写入 `/tmp` 下的临时 HOME，验证缺少 dotconfig 引擎时拒绝复制私有副本、设备钩子安装、
幂等性、已有 watcher 保留和 `xprofile` 符号链接处理；不会启动真实 watcher 或改变显示布局。
xdisplay 的状态机、布局、适配器、配置和自定义布局测试只在 dotconfig 仓库维护。所有权边界见
[`docs/planning/display-integration.md`](../docs/planning/display-integration.md)。

不得提交测试运行产生的锁文件、日志、runtime 目录或本机绝对路径。

Picom 用户配置安装器测试：

```sh
tests/picom/run-install-tests.sh
tests/picom/run-session-tests.sh
```

这些测试只写入 `/tmp` 下的临时 HOME，并用假命令验证 Picom 优先级、xcompmgr 回退和单实例，
不启动真实 Picom 或 xcompmgr。

测试数量以各脚本运行时输出为准，不在本文复制易过时的计数。
