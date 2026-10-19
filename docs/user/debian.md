# Debian 安装指南

这是 Debian 的平行档案，不改 [Gentoo 现场](gentoo.md) 的结论。当前运行态仍以
[status.md](../project/status.md) 为准。本页只写已经有脚本和已验证内核的路径，不代替受监督安装。

## 前置

- Debian Trixie。
- 内核与 `6.12` 同代，并且不高于 `6.17`。仓库里没有自动拒绝更高版本的门。已验证的只有
  `6.12.107+deb13-amd64` 和 `6.12.111+deb13-amd64`。没有在其他 `≤6.17` 内核上装过，不能把边界写成已验收。
- 来源包放进 `debs/`，不进 git：

```text
debs/fantgpu-fh2m_3.3.8.126-driver-linux-desktop-sp-generic_amd64.deb
SHA-256: 6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775bebc2d28fd11b
```

构建前核对这个 SHA。O 线 deb 不混用。`vendor/fantgpu/` 由这份来源包物化，也不进 git。

## 构建

`VERSION=5.0.0-i12` 时脚本自己把血统设成 fantgpu。`FANT_LINEAGE=1` 与这个版本一致；换版本时以脚本覆盖后的值为准。

```sh
FANT_LINEAGE=1 VERSION=5.0.0-i12 SOURCE_DATE_EPOCH=1790812800 tools/debian/build-driver.sh
```

产物是 `.build/work/driver-build/fantgpu-fh2m-trixie_5.0.0-i12.deb`。包内源码到 `m-035-private-accessor`。
`m-036-bf-shf-rename` 不在构建器里。当前 Debian `6.12.111` 上加载的模块是这份树再加构建器外的 `m-036-bf-shf-rename`。
只装这个 deb，不会得到那份已加载模块。

构建不安装，不热切，不重启。

## 安装

先有人在物理 TTY。缺这个就不开始。七项包外预检全绿才可以 `dpkg`，任何一项失败就停：

1. 候选身份：整 deb SHA、版本、epoch，以及解包后的 postinst/prerm 和源码 digest。
2. 现场内核：只枚举已安装的 `linux-image-*`。不删核，不改 GRUB 默认项。
3. 前驱 prerm 审计。
4. 备份：注册、逐核模块、depmod、initramfs、包数据库和 GRUB。
5. 回滚卡 `4.0.2-i3` 实物 SHA
   `177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662`。
6. Kylin 钩子隔离根实跑。
7. 窗口、电源、落盘、root 和信任条件。

```sh
sudo dpkg -i .build/work/driver-build/fantgpu-fh2m-trixie_5.0.0-i12.deb
```

安装后的重启另要确认。不要热卸载。不要改 GRUB。

## 内核补丁

`tools/debian/build-linux-patched-modules.sh` 和 `tools/debian/install-kernel-patches.sh` 现在只处理
`components/linux` 的 001 和 002，产物是 `xhci-pci.ko` 与 `battery.ko`，装到 `updates/r51`。
003 是内建 `pinctrl-amd`，004 是 `snd-hda-intel` 模块。这两个脚本不编译、不安装 003/004。
005 只给 `CONFIG_DRM_DISPLAY_HDMI_HELPER` 加菜单提示，要打进内核源码后整核重编。这两个脚本也不处理 005。
001/002 的基线是 Debian `linux-source-6.12` `6.12.107-1`。换内核必须重打、重编，禁止复用另一内核的模块。
002 是机型专用 workaround，不能直接套到别的内核版本。

构建用普通用户。安装脚本要求物理 TTY 和交流电在线，并且不重启。

```sh
tools/debian/build-linux-patched-modules.sh --kernel-release "$(uname -r)"
sudo tools/debian/install-kernel-patches.sh --evidence .build/linux-patched-modules/<本次目录>
```

先 `--preflight` 也可以，它只做只读准入。模块进 initramfs 之后，要下次启动才替换已经加载的内核内模块。
两个 `1d94:148c` 的验收值是 `quirks=0x90`。

## 验收

重启后、不触发 PM：

- `tools/debian/check-fantgpu-runtime-health.sh --dmesg <本次 dmesg> --status-file <Driver/Firmware 状态>`。
  这个脚本不数八项计数。八项是 `Server`、`HWR`、`CRR`、`SLR`、`WGP`、`TRP`、`FWF`、`APM`，八行都要在，而且都是 0。缺行不是 0。
- 本次 Xorg 日志有 glamor 和 AIGLX。旧启动的日志不算。
- `/proc/bus/input/devices` 是 18 个设备，18 行 Handlers 含 `fant_input`。这只是挂接，不是逐项击键。
- `BAT0` 在。丢失时用 `tools/debian/restore-bat0.sh` 恢复并留下它自己的证据，再判定。
- `tools/debian/check-hygon-xhci-resume-fix.sh` 看 reset-on-resume 位。本机判据仍是两个控制器都是 `0x90`。

R5 的 3 次 devices 加 1 次 normal deep 没有包含在本页。要跑须另获授权。不写 `pm_test`，不跑 watchdog。
`hwinfo_g0m.bin` 仍缺，不把这项写成通过。
