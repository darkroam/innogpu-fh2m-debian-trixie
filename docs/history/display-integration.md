# 显示管理接入历史

## 当前状态

状态：历史计划已完成并在 2026-08-14 重新划定所有权。

2026-07-16 本项目曾从 dotconfig 提交 `5628c6e` 吸纳 `xdisplay.sh`、`displayselect` 和 fixture，用于
验证 Innogpu 的热插拔、合盖、stale 输出和模式恢复。后续 dotconfig 已将引擎演进为独立命令、共享库、
配置系统、适配器和自定义布局；继续在两个仓库保留副本会造成实现、测试和文档漂移。

因此当前规则为：dotconfig 是 xdisplay 的唯一源码权威，本项目只维护设备接入。原吸纳代码和引擎
fixture 已从本仓库删除，历史提交仍可用于追溯当时的验证结果。

## 当前文件映射

| 来源/所有者 | Innogpu 接入 | 处理 |
| --- | --- | --- |
| dotconfig `.local/bin/xdisplay` 与 `.local/lib/xdisplay/` | 无代码副本 | 引擎、配置和测试只在 dotconfig 维护 |
| dotconfig `.local/bin/displayselect` | 无代码副本 | 手动布局和自定义布局只在 dotconfig 维护 |
| dotconfig X11 会话 | `scripts/xdisplay-session.sh` | 注入本设备环境变量并启动已有 xdisplay |
| Innogpu 固定 modeline | `scripts/restore-dp1-mode-x11.sh` | 保留为设备恢复钩子 |
| Innogpu 用户接入 | `scripts/install-xdisplay-user.sh` | 只安装恢复钩子和会话 source 块 |
| Innogpu 边界测试 | `tests/xdisplay/run-install-tests.sh` | 只验证接入幂等和不覆盖 dotconfig 文件 |

完整 `xprofile` 不进入本项目，因为其中还包含输入法、代理、壁纸、Picom 和其他个人配置。活动
`/etc/X11`、logind 和 udev 状态只记录关系，不直接复制。

## 历史验证结果

原吸纳阶段曾完成：

- stage2 状态、锁和可选后处理 12 项；
- watcher 生命周期 4 项；
- stage4 显示回归 11 项；
- 临时 HOME 用户安装器 4 项；
- 当前 X11 只读状态比较和来源校验。

这些结果只说明 2026-07-16 的历史快照，不再代表当前 dotconfig 引擎。当前状态机、适配器、配置、
多外屏和自定义布局的验证结果以 dotconfig 的
`.local/share/test/display/xdisplay-adapter.sh` 为准。

## 当前验收门槛

1. `install-xdisplay-user.sh` 在缺少 dotconfig xdisplay 时明确失败，不安装私有副本。
2. 接入脚本只安装设备恢复钩子和 `innogpu-display-session.sh`。
3. 重复安装不重复写入 xprofile，不替换符号链接。
4. 已存在的 xdisplay、displayselect、共享库、配置和测试不得被修改。
5. 设备恢复失败不得阻塞 dotconfig 引擎的通用 RandR 降级路径。
6. 只读验证可运行 `xdisplay status`；热插拔、合盖和 modeset 单独记录实机结果。

## 回退

若 Innogpu 接入异常：

1. 停止当前 watcher，避免两条 RandR 写入链路并存；
2. 从 xprofile 删除或注释 `BEGIN INNOGPU DISPLAY SESSION` 标记块；
3. 保留 dotconfig 的 xdisplay 和 displayselect，不通过重装 DKMS 修复纯布局问题；
4. 使用 displayselect、TTY 或 SSH 保持至少一个可见输出；
5. 驱动包异常时按 [`user/recovery.md`](../user/recovery.md) 的版本化链路回退。
