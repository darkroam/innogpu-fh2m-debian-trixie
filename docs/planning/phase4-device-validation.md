# Phase 4 实机候选验证（准备包）

**状态**：Phase 3 已通过监督评审（2026-08-21）；本准备包已通过复核（含 chroot 恢复先 `lsblk -f`
确认根分区）。本文件是 Phase 4 的**离线准备**：回退包、恢复通道、失败恢复命令与安装前后验证
清单。**安装 4.0.0-i1 仍未执行**，需要监督对"安装 + 模块切换 + 重启"单独明确批准。本文档所有
行为承诺均需在实机执行阶段用命令与输出证明。

## 一、候选与回退包（已核实）

| 项 | 值 |
| --- | --- |
| 候选包 | `build/innogpu-fh2m-trixie_4.0.0-i1.deb`（新架构构建器，可复现 epoch 1787342400） |
| 候选 SHA-256 | `68aea6c07842a0def97d18de5385802175290cb9752571b157250eb38fa68735`（双构建逐字一致） |
| 候选特点 | 192 项 manifest；无 `.o.cmd` 构建产物；模块符号与 p27 一致（12839 defined / 768 imported / CRC 一致） |
| 回退包 | `debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb` |
| 回退包 SHA-256 | `f384159751fed249263591ff46758bb32327d0048e0669747050b66db1e33c6a`（2026-08-21 现场核实与 [release-review](release-review-2026-08-20.md) 记录一致） |
| 版本排序 | `dpkg --compare-versions 3.3.3.42-patched-27 lt 4.0.0-i1` = true（回退构成降级） |

## 二、恢复通道（已只读核实，2026-08-21）

| 通道 | 状态 | 核实命令 |
| --- | --- | --- |
| SSH | `ssh.service` active | `systemctl is-active ssh` |
| 真实 TTY | `getty@tty1`、`getty@tty2` active | `systemctl list-units | grep getty` |
| 本地会话 | `ok` @ seat0 / tty1 | `who` |
| 磁盘 | `/` 17% 已用（145G 可用） | `df -h /` |
| 内核头 | `/lib/modules/6.12.101+deb13-amd64/build` 存在 | `ls -d /lib/modules/*/build` |

注意：本次运行环境（沙箱）无法看到 `/dev/dri` 等设备节点；所有运行时探测必须在真实会话
（seat0/tty1 或 SSH 的实机侧）执行，沙箱观测值不作为设备状态证据。

## 三、失败恢复命令（安装批准后执行）

### 3.1 标准回退（降级回 p27）

```bash
# 1) 恢复通道（任一）：SSH 或 tty1/tty2 登录
# 2) 若 DKMS 残留新版本模块，先清理
sudo dkms remove -m innogpu-kernel -v 2.2 --all || true
# 3) 降级安装回退包（apt 默认拒绝降级，必须 --allow-downgrades）
sudo apt install --allow-downgrades ./debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb
# 4) 重建 initramfs 与模块依赖
sudo update-initramfs -u -k 6.12.101+deb13-amd64
sudo depmod -a 6.12.101+deb13-amd64
# 5) 重启
sudo systemctl reboot
```

### 3.2 模块加载失败 / 黑屏

```bash
# SSH/TTY 登录后：
sudo dmesg | grep -iE 'innogpu|innosrvkm|pvr' | tail -30   # 定位加载错误
sudo modprobe -r innogpu 2>/dev/null || true
sudo rm -f /etc/modprobe.d/innogpu.conf.disabled            # 不误删配置
# 若为驱动初始化挂起：回退到 3.1
```

### 3.3 initramfs 阶段无法进入系统

```bash
# 从 GRUB recovery 模式或 live USB 进入后 chroot。
# 第一步必须先确认根分区：不能硬编码设备路径，故障环境的分区布局可能不同。
lsblk -f            # 找到 FSTYPE=xfs/ext4 且挂载点为 / 的分区，记下其设备（如 /dev/nvme0n1p2）
ROOT_DEV=/dev/nvme0n1p2   # ← 以 lsblk -f 实际输出为准，禁止直接照抄
mount "$ROOT_DEV" /mnt
mount --bind /dev /mnt/dev && mount --bind /proc /mnt/proc && mount --bind /sys /mnt/sys
chroot /mnt /bin/bash
# 删除候选 DKMS 模块并恢复 p27
dkms remove -m innogpu-kernel -v 2.2 --all || true
apt install --allow-downgrades ./debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb
update-initramfs -u -k 6.12.101+deb13-amd64
exit
```

## 四、安装前基线采集（候选安装前，实机会话执行）

| # | 项 | 命令/方式 | 记录 |
| --- | --- | --- | --- |
| B1 | 当前包版本 | `dpkg -l | grep innogpu` | 期望 3.3.3.42-patched-27 |
| B2 | 已加载模块 | `lsmod | grep -E '^innogpu'` | 期望 innogpu 已加载 |
| B3 | 模块文件 | `ls -l /lib/modules/$(uname -r)/updates/dkms/` | innogpu.ko.xz |
| B4 | DKMS 注册 | `dkms status`（/usr/sbin/dkms） | innogpu-kernel/2.2 |
| B5 | modprobe 配置 | `cat /etc/modprobe.d/innogpu.conf` | options innogpu firmware_en=1 |
| B6 | initramfs 快照 | `lsinitramfs /boot/initrd.img-$(uname -r) | grep -i innogpu` | 记录当前条目 |
| B7 | DRM/fbdev 节点 | `ls -la /dev/dri /dev/fb*`（真实会话） | card0/renderD128/fb0 等 |
| B8 | Xorg/GLX | `scripts/test-current-xorg-hwgl-runtime.sh` / `check-desktop-hwgl.sh` | 硬件 GL 路径 |
| B9 | 音频 | `aplay -l`、`pactl list sinks short` | 内置 HDA 默认 sink |
| B10 | Picom | `pgrep -a picom` | patched v13 运行中 |
| B11 | 显示切换 | xdisplay 状态、`xrandr -q`（真实会话） | 当前布局快照 |
| B12 | 用户态一致性 | `scripts/check-deepin-userspace-coherence.sh` | PASS |

## 五、安装后验证清单（4.0.0-i1 安装 + 受控重启后）

| # | 项 | 通过判据 |
| --- | --- | --- |
| A1 | 包版本 | `dpkg -l` = 4.0.0-i1 |
| A2 | DKMS 构建 | `dkms status` = innogpu-kernel/2.2 (installed)；构建日志无错误 |
| A3 | 模块加载与 vermagic | `modinfo /lib/modules/$(uname -r)/updates/dkms/innogpu.ko.xz` vermagic = 6.12.101+deb13-amd64 SMP preempt mod_unload modversions；`lsmod` innogpu 已加载 |
| A4 | Driver/Firmware | dmesg PVR 进入 ACTIVE、Driver/Firmware OK、错误计数 0（对齐 patch-024 验收） |
| A5 | DRM/fbdev | /dev/dri card0 + renderD128、/dev/fb0 可用（对齐 patch-024 验收） |
| A6 | Xorg/GLX | `check-desktop-hwgl.sh`、`test-current-xorg-hwgl-runtime.sh`：硬件 GL 路径、glxinfo renderer 为 InnoGPU |
| A7 | 用户态一致性 | `check-deepin-userspace-coherence.sh` PASS（Deepin 202504 载荷优先） |
| A8 | 音频 | HDA 默认 sink、PipeWire 正常（对齐 patch-021 验收） |
| A9 | Picom | patched v13 使用 Innogpu GLX（对齐 compositor-management.md） |
| A10 | 显示切换 | xdisplay 会话、`XDISPLAY_INTERNAL_OUTPUTS`、恢复命令兼容 |
| A11 | 符号/回归 | PDP READ/WRITE 回归（patch-025/026/027 相关路径）正常 |
| A12 | 重启持久性 | `check-post-reboot-hwgl.sh`（重启后硬件 GL 持久） |

## 六、执行顺序与回退演练（2026-08-21 监督确认的完整流程）

**回退演练本身包含"先安装候选并重启，再回退到 p27"，因此在候选尚未安装时无法完成完整回退演练。**
获得安装授权后严格按以下顺序执行（不可调整）：

1. **B1–B12 基线采集**（§四，实机会话）；
2. **安装 4.0.0-i1 并受控重启**（§3.1 前的候选安装）；
3. **A1–A12 初次验收**（§五）；
4. **回退到 p27 并重启**（§3.1 标准回退命令）；
5. **验证 p27 恢复**（包版本、模块、DRM/fbdev、Xorg/GLX、音频、Picom 恢复，对齐 patched-27 基线）；
6. **如需继续，重新安装候选**完成正式验证；全程记录每次重启的 TTY/Xorg/GL 状态。

"回退演练"对应步骤 2→5（安装候选后回退并验证恢复）；步骤 6 仅在继续验证时执行。

## 七、执行纪律

- 安装、模块切换、重启、tag 移动、旧流程删除：**均需监督单独批准**；本文件不构成安装授权。
- 任一验证失败：先进入恢复路径（§三），再做事故记录；不做模块热切换。
- 沙箱观测值（无 /dev/dri 等）不作为设备状态证据；实机探测须在真实会话执行。
- 无法验证的项标记 UNVERIFIED 并在交付报告中列出。
