# New Debian Device Install

目标：在新设备重装 Debian Trixie 后，clone 本仓库，再把 release 中的 `.deb` 包下载到仓库根目录，即可安装当前验证过的 FH2M 显卡方案。

## Required Release Files

这些文件不纳入 git，需要从 release 独立下载后放到仓库根目录：

- `innogpu-fh2m-trixie_3.3.3.42-patched-8.deb`
- `innogpu-fh2m-trixie_3.3.3.42-patched-17.deb`
- `innogpu-fh2m_20250421190503-debug_amd64.deb`

`patched-17` 包含 DKMS 源码、firmware、模块配置和辅助脚本。Deepin 202504 deb 用于提供硬件 GL/DDX 用户态库。

确认文件已就位：

```bash
cd "$INNOGPU_ROOT"
ls -lh \
  innogpu-fh2m-trixie_3.3.3.42-patched-8.deb \
  innogpu-fh2m-trixie_3.3.3.42-patched-17.deb \
  innogpu-fh2m_20250421190503-debug_amd64.deb
```

这些 `.deb` 会被 `.gitignore` 忽略，不会进入代码库。

Deepin deb 的默认位置是仓库根目录：

```text
./innogpu-fh2m_20250421190503-debug_amd64.deb
```

执行 `scripts/prepare-deepin-userspace-root.sh` 后会生成：

```text
./third_party/innogpu-fh2m-deepin-202504/root/
```

其中 patched-17 构建需要的 Deepin DKMS 源码位于：

```text
./third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2/
```

`third_party/` 是生成目录，不需要同步到代码库。

## Repository Layout

```text
.
├── patches/      # DKMS/kernel patches for Debian 6.12 and this device
├── scripts/      # build, install, rollback, verification, recovery helpers
├── tools/        # small EGL/GBM/X11 probe programs
├── docs/         # installation and maintenance notes
├── baselines/    # final pass/fail evidence
├── third_party/  # generated Deepin userspace extraction, ignored by git
└── *.deb         # release artifacts downloaded to repo root, ignored by git
```

新设备安装只依赖两类内容：

- Git 仓库内容：脚本、补丁、文档、探针源码。
- Release 包：根目录下 3 个 `.deb`，由用户独立下载，不提交进仓库。

## External Debian Packages

新系统仍需要 Debian 官方包，通常通过 apt 安装：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-prereqs-debian.sh
```

如果没有网络，需要提前准备这些 Debian 包及其依赖：`dkms`、`build-essential`、当前内核 headers、`initramfs-tools`、`xserver-xorg-core`、`xinit`、`mesa-utils`、`x11-utils`、`x11-xserver-utils`、`dwm`。

## Install patched-17

```bash
export INNOGPU_ROOT="$HOME/src/innogpu-fh2m-debian-trixie"
cd "$INNOGPU_ROOT"
sudo scripts/install-patched17-and-check.sh
sudo reboot
```

重启后先确认 tty1、Xorg、dwm 可正常工作，并验证 patched-17 的基础状态：

```bash
cd "$INNOGPU_ROOT"
scripts/verify-install-status.sh 3.3.3.42-patched-17
```

基础状态通过后，再启用 Deepin 硬件 GL 用户态：

```bash
cd "$INNOGPU_ROOT"
scripts/prepare-deepin-userspace-root.sh
sudo scripts/run-local-ddx-vt-test.sh
sudo scripts/install-deepin-desktop-hwgl-trial.sh
sudo reboot
```

重启后验证硬件 GL 状态：

```bash
cd "$INNOGPU_ROOT"
scripts/verify-install-status.sh 3.3.3.42-patched-17
scripts/check-desktop-hwgl.sh
sudo scripts/check-post-reboot-hwgl.sh
```

期望：

```text
PASS_DESKTOP_HWGL
PASS_POST_REBOOT_HWGL
```

## Internal Speaker Audio

本设备内置喇叭使用 HDA codec `Conexant SN6180`。PCI 设备是：

```text
0000:06:00.6 Audio device [1d94:14c9]
```

Debian 的 `snd_hda_intel` 模块存在，但默认 PCI alias 不匹配 `1d94:14c9`，所以新系统上通常会出现：

- `InnosiliconCard` 只显示 `DP0/HDMI0/HDMI1` 数字音频。
- 内置喇叭没有对应的 `SN6180 Analog` 输出。
- `lspci -nnk -s 06:00.6` 没有 `Kernel driver in use`。

安装持久化修复：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-hygon-hda-audio.sh
```

如果需要直接播放一次测试音：

```bash
sudo scripts/install-hygon-hda-audio.sh --test-sound
```

安装后验证：

```bash
lspci -nnk -s 06:00.6
aplay -l
amixer -c Intel sget Speaker
wpctl status
systemctl status hygon-hda-audio.service --no-pager
systemctl --user status hygon-hda-audio-user.service --no-pager
```

期望看到：

```text
Kernel driver in use: snd_hda_intel
card ... Intel [HDA Intel], device 0: SN6180 Analog [SN6180 Analog]
Speaker ... [on]
hygon-hda-audio.service ... enabled/active
hygon-hda-audio-user.service ... enabled/inactive or active after successful oneshot run
```

脚本写入的系统文件：

```text
/etc/modules-load.d/hygon-hda-audio.conf
/etc/systemd/system/hygon-hda-audio.service
```

脚本会直接维护当前用户的 ALSA/PipeWire 配置：

```text
~/.config/alsa/asoundrc
~/.asoundrc -> ~/.config/alsa/asoundrc
~/.config/systemd/user/hygon-hda-audio-user.service
~/.local/bin/hygon-hda-audio-user-apply
```

不要全局导出 `ALSA_CONFIG_PATH="$XDG_CONFIG_HOME/alsa/asoundrc"`。`ALSA_CONFIG_PATH` 会替换系统 `/usr/share/alsa/alsa.conf`，不是追加配置，会导致 PipeWire/WirePlumber 只能看到虚拟输出或 `off` profile。脚本会注释掉 `.profile`/`.zprofile` 中的该导出，并使用 `~/.asoundrc` 标准方式加载用户 ALSA 默认输出。

## Installation Status Checks

`scripts/verify-install-status.sh` 是只读检查脚本，不会修改系统。它检查：

- `Package`：`innogpu-fh2m-trixie` 是否安装，版本是否符合期望。
- `DKMS`：当前内核 `$(uname -r)` 是否已经安装 `innogpu-kernel/2.2` DKMS 模块。
- `Kernel Module`：`innogpu` 是否已加载，Driver/Firmware 是否为 OK。
- `Device Nodes`：`/dev/dri/card0`、`/dev/dri/renderD128`、`/dev/fb0` 是否存在。
- `Boot Autoload`：`/etc/modules-load.d/innogpu.conf` 是否启用开机加载。
- `Xorg And Userspace`：当前 Xorg 是 `innogpu` DDX 还是 `modesetting` 软件路径，Deepin 硬件 GL 用户态文件是否齐全。
- `Desktop Runtime`：如果当前会话能连接 Xorg，会只读检查 renderer、DRI3、GLX、Present。

结果含义：

- `PASS_INSTALL_STATUS`：包、DKMS、驱动状态、关键节点没有硬失败。
- `FAIL_INSTALL_STATUS`：至少一个关键项失败，需要先处理再重启或启用硬件 GL。
- `WARN`：当前状态可能是合理的中间态，例如刚安装后还没重启、硬件 GL 用户态尚未启用、当前没有 Xorg 会话，或当前在 Codex/bwrap 隔离环境中看不到真实 `/dev/dri`。

## Install patched-8

`patched-8` 是原始稳定回退点，适合在 `patched-17` 出问题时恢复基本可启动状态：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/install-patched8-and-check.sh
sudo reboot
```

重启后验证：

```bash
cd "$INNOGPU_ROOT"
scripts/verify-install-status.sh 3.3.3.42-patched-8
```

## Uninstall patched-17

仅当当前安装版本确实是 `3.3.3.42-patched-17` 时执行：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/uninstall-patched17.sh
sudo reboot
```

卸载后验证：

```bash
! dpkg-query -W innogpu-fh2m-trixie
lsmod | grep '^innogpu' || true
```

## Uninstall patched-8

仅当当前安装版本确实是 `3.3.3.42-patched-8` 时执行：

```bash
cd "$INNOGPU_ROOT"
sudo scripts/uninstall-patched8.sh
sudo reboot
```

卸载后验证同 patched-17：包应不存在，重启后 `innogpu` 模块不应继续加载。

## Emergency Rollback

如果 `patched-17` 重启后显示异常：

```bash
cd "$INNOGPU_ROOT"
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
sudo scripts/disable-incompatible-userspace.sh
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a "$(uname -r)"
sudo update-initramfs -u -k "$(uname -r)"
sudo reboot
```
