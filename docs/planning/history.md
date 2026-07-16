# 实施历史

## 2026-07-08 仓库清理

- 保留 `patched-8` 回退点和 `patched-17` 当前成功点。
- 删除失败或过渡版本、重复日志和包含本机路径的大型原始基线。
- 将外部 deb 改为 release 下载后放到仓库根目录，并由 `.gitignore` 排除。
- 清理脚本中的固定 home 路径，建立 `$INNOGPU_ROOT`、`$INNOGPU_X_USER` 等约定。

完整历史记录见 `../archive/cleanup-20260708.md`。

## 2026-07-09 内置喇叭

- 确认 Innogpu 声卡只负责 DP/HDMI 数字音频。
- 将 PCI `1d94:14c9` 绑定到 `snd_hda_intel`，识别 Conexant SN6180。
- 修复全局 `ALSA_CONFIG_PATH` 导致的 PipeWire/WirePlumber profile 失败。
- 增加系统级和用户级服务，重启后内置喇叭与默认 PipeWire sink 验证通过。
- 对应提交：`9cdc410`。

## 2026-07-16 显示来源实现

本机 dotfiles 仓库完成显示 watcher 改造，来源提交为：

- `0b40ac7 Refactor X11 display observation`
- `1f8790b Document display framebuffer diagnostics`
- `5628c6e Fix dock display convergence`

该实现已在本机通过 fixture、当前 X11、热插拔、开合盖和重启验证。本项目尚未吸纳代码，迁移状态以
`display-integration.md` 为准。
