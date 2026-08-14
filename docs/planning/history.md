# 实施历史

## 2026-08-14 文档与 release 目录重构

- 将补丁说明拆为 `docs/patches/patch-*.md`，并为 patched-17 framebuffer、patched-18 用户态混配、
  shader 固件、patched-20 运行时和 Picom 能力特例建立独立事故记录。
- 增加当前状态唯一摘要、脚本职责索引和文档阅读入口；旧 `scripts/` 路径保持为稳定 API，不做
  物理搬迁。
- 将 `debs/` 定义为本地 release/构建目录，仅跟踪说明文件，`.deb` 和 `third_party/` 继续忽略。
- 集中整理开发、测试、隐私、载荷基线和回退约束，后续行为修改必须先更新文档再改代码。

## 2026-07-08 仓库清理

- 保留 `patched-8` 回退点和 `patched-17` 当前成功点。
- 删除失败或过渡版本、重复日志和包含本机路径的大型原始基线。
- 将外部 deb 改为 release 下载后放到仓库根目录，并由 `.gitignore` 排除（当前已迁移为 `debs/`，
  该条保留为当时的历史状态）。
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

该实现已在本机通过 fixture、当前 X11、热插拔、开合盖和重启验证。

## 2026-07-16 显示代码吸纳

- 吸纳通用 `xdisplay.sh`、共享锁 `displayselect`、设备恢复钩子和净化后的 fixture。
- 删除旧的硬编码 `xdisplay.sh.with-innogpu-restore`。
- 新增可独立测试的用户会话安装器，并接入 patched-17 安装、软件 Xorg 准备和 deb 构建流程。
- fixture 共 27 项、用户安装器 4 项通过；patched-17 测试包在 `/tmp` 完整重建并核对内容。
- 当前 X11 只读比较确认仓库 watcher 与运行来源一致，合盖外屏 `health=ready`。

完整迁移记录见 `display-integration.md`。

## 2026-07-17 Picom GLX 兼容流程吸纳

- 从 dotfiles 提交 `7747c2d` 和当前配置恢复 Picom/xcompmgr 启动、v13 动画、发白排查、st 不透明
  规则和最终模糊配置的处理历史。
- 从上游 Picom `6d676824` 的本机源码差异导出 explicit uniform location shader 探测 patch。
- 保存与当前运行文件逐字一致的 `picom.conf`，增加独立会话、依赖、构建和用户安装脚本。
- 在 `/tmp` 干净 clone 中完成 patch 和 46 步构建；GLX diagnostics 确认为 Fantasy II-M 硬件加速。
- 用户安装器和会话选择各 3 项通过，当前运行 Picom 未停止或替换。

完整记录见 `picom-integration.md`。

## 2026-08-13 Picom GLX 能力判断修复复核

- 反查 `third_party/` 中的 Innogpu 用户态库：当前仓库只保存预编译 `.so`，没有可直接修改的
  OpenGL 扩展表源码；驱动库字符串和 GLSL 诊断表明 `layout(location = ...)` 编译能力存在，
  但运行时扩展列表未暴露该名称。
- 保持驱动包不变，采用现有 `patches/picom/001-probe-explicit-uniform-location.patch`，让
  Picom 在扩展缺失时编译最小 shader，失败仍阻止 GLX，成功才继续。
- Picom release/debugoptimized 构建、单元测试和真实 `DISPLAY=:0` GLX 启动验证通过；运行日志
  只出现兼容 warning。该特例仍由 Picom 用户态 patch 所有，未加入 DKMS deb。
