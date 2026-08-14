# 状态验证

## 驱动、DKMS 与节点

当前本机验收目标是 patched-20：

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-20
cat /proc/driver/innogpu/gpu00/status
ls -l /dev/dri /dev/fb0
```

验证 patched-17 回退时把期望版本改为 `3.3.3.42-patched-17`。期望 Driver/Firmware 为 OK，并存在
`card0`、`renderD128` 和 `fb0`；版本不匹配时不能用当前模块结果证明新包通过。

## patched-21 分阶段验收

patched-21 的完整候选定义见
[`patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)。它必须把包文件验证和
活动系统验证分开：前者只证明 deb 内容符合当前源码边界，后者才证明新驱动在目标机器运行。

### 阶段 A：构建与离线包验证

本阶段允许在当前 p20 会话中执行，不安装 deb、不运行 maintainer scripts、不调用 DKMS：

```sh
tests/package/run-boundary-tests.sh
scripts/build-patched21-deepin-release-candidate.sh
scripts/check-release-package.sh \
  debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
dpkg-deb -f debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb \
  Package Version Architecture Installed-Size
sha256sum debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
```

验收标准：包名、版本、架构正确，release 审计返回 `PASS_RELEASE_PACKAGE_BOUNDARIES`，记录输出包
SHA-256；同时确认包内没有 xdisplay 引擎副本、Kylin/实验安装器和直接 GPU PLL 热补丁入口。

本阶段完成后文档只能标记：

```text
BUILD: PASS
PACKAGE_BOUNDARY: PASS
INSTALL: NOT_INSTALLED
REBOOT: NOT_REBOOTED
RUNTIME_VALIDATION: PENDING
```

### 阶段 B：未来安装前记录与回退准备

只有在阶段 A 的实际哈希和审计结果已写回文档并审阅后才进入本阶段。执行安装前保存：

```sh
dpkg-query -W -f='${Package} ${Version}\n' innogpu-fh2m-trixie
uname -r
dkms status innogpu-kernel
scripts/verify-install-status.sh 3.3.3.42-patched-20
```

同时确认 patched-17 回退 deb、SSH 或真实 TTY、`scripts/install-patched17-and-check.sh` 可用。
安装 p21 会修改 DKMS、用户态图形库、initramfs 和后续启动行为，必须另行明确执行，不属于离线验收。

### 阶段 C：未来安装和重启后的版本一致性

受控安装并重启后，先检查版本身份，任何一处仍为 p20 都不能继续宣称 p21 运行通过：

```sh
scripts/verify-install-status.sh 3.3.3.42-patched-21
dpkg-query -W -f='${Version}\n' innogpu-fh2m-trixie
dkms status innogpu-kernel
modinfo -F filename innogpu
cat /proc/driver/innogpu/gpu00/status
```

预期安装包和当前内核的 DKMS 实例均为 p21，活动 `innogpu` 来自该内核模块目录，Driver/Firmware
状态为 OK。只完成安装但未重启时，活动模块仍可能是旧版本，此时状态保持 `NOT_REBOOTED`。

### 阶段 D：未来固件、PVR、节点和日志

```sh
sudo journalctl -b -k --no-pager | \
  grep -E 'innogpu|PVR_K|Firmware image|Shader binary|srvkm_init'
ls -l /dev/dri /dev/dri/by-path /dev/fb0
```

预期 `fh2m.fw` 和 `fh2m.sh` 均成功加载，PVR 可打开并进入 ACTIVE，DRM/render/fbdev 节点存在。
p21 关闭 `patch-008`，因此不应再出现该补丁新增的成对
`innogpu: srvkm_init module=... state_before/state_after` 高频日志。这里检查的是特定诊断行消失，
不是要求内核日志完全没有 PVR 信息。

### 阶段 E：未来 Xorg、硬件 GL、fbterm 与桌面

依次执行下文的 Xorg/硬件 GL 检查；只有临时 Xorg、`xdpyinfo`、`glxinfo` 和当前桌面检查均通过，
才能标记硬件 GL 通过。然后在未被 Xorg 占用的真实 VT 运行 `fbterm`，确认进入、绘制和退出。

最后检查 dotconfig xdisplay、开合盖/热插拔、Picom 和正常桌面。显示管理状态必须用物理屏幕、
`xdisplay status` 和 RandR 共同确认；Picom 或 xdisplay 的通过不能替代驱动、PVR 或 fbterm 证据。

阶段 C-E 全部完成后，才允许把 p21 标记为 `RUNTIME_VALIDATION: PASS`。任一步失败应保存最小诊断、
执行回退并在 `docs/incidents/` 新建记录。

## Xorg 与硬件 GL

```sh
scripts/check-innogpu-progress.sh
scripts/check-desktop-hwgl.sh
sudo scripts/check-post-reboot-hwgl.sh
sudo scripts/test-current-xorg-hwgl-runtime.sh
```

patched-20 的验收标志包括 `PASS_DESKTOP_HWGL`、`PASS_POST_REBOOT_HWGL` 和
`PASS_CURRENT_XORG_HWGL_RUNTIME`。应确认 renderer 为 `Fantasy II-M`、direct rendering 启用，并有
DRI3、GLX、Present 和 RANDR。提交中的 baseline 只是历史证据，不能替代当前设备检查。

## framebuffer 与真实 VT

patched-20 必须在真实 VT 验证 fbdev mmap 和 fbterm。通过 SSH 保存内核日志后，切换到未运行 Xorg
的真实 VT 执行：

```sh
fbterm
```

普通用户出现“cannot change kernel keymap table”只表示 fbterm 快捷键受限；能够进入、绘制并正常
退出即通过。段错误时使用 `strace -ff -o "$HOME/fbterm.strace" fbterm` 保存本机证据，但不要将
trace、用户名或绝对 home 路径提交到仓库。

## 显示管理接入

xdisplay 由 dotconfig 维护。在 X11 会话中使用 dotconfig 的命令只读检查：

```sh
xdisplay status
xrandr --current
cat /sys/class/tty/tty0/active
```

实机验证必须同时看状态输出、RandR 和物理屏幕，不能只看 `connected`。xdisplay 的状态机、适配器、
多外屏、配置和自定义布局测试在 dotconfig 运行：

```text
.local/share/test/display/xdisplay-adapter.sh
```

Innogpu 仓库只运行不会启动 watcher、不会改变布局的接入测试：

```sh
tests/xdisplay/run-install-tests.sh
```

## 内置音频

```sh
lspci -nnk -s 06:00.6
aplay -l
amixer -c Intel sget Speaker
wpctl status
systemctl status hygon-hda-audio.service --no-pager
systemctl --user status hygon-hda-audio-user.service --no-pager
```

期望 HDA 使用 `snd_hda_intel`，ALSA 出现 `SN6180 Analog`，Speaker 为 `[on]`，PipeWire 默认 sink
是 `HDA Intel 模拟立体声`。
