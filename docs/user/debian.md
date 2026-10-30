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
`components/linux` 的 001 和 002，产物是 `xhci-pci.ko` 与 `battery.ko`，装到 `updates/r52`。安装不删除残留的 `updates/r51`。
003 是内建 `pinctrl-amd`，004 是 `snd-hda-intel` 模块。这两个脚本不编译、不安装 003/004。
005 只给 `CONFIG_DRM_DISPLAY_HDMI_HELPER` 加菜单提示，006 只给 `CONFIG_VMAP_PFN` 加菜单提示。都要打进内核源码后整核重编。这两个脚本也不处理 005 和 006。
2026-10-20 实测：111 上的 001/002 在 `updates/r52`。脚本现写入 `updates/r52`。本轮未实机验证。
003 未装，触摸板走 HYGO0010，正常。004 未装，板载 SN6180（`1d94:14c9`）没有 quirk 也能出声。
005/006 的符号在发行版 config 里已经是 y，提示补丁不适用。不补装。
001/002 的基线是 Debian `linux-source-6.12` `6.12.107-1`。换内核必须重打、重编，禁止复用另一内核的模块。
002 是机型专用 workaround，不能直接套到别的内核版本。

构建用普通用户。安装脚本要求物理 TTY 和交流电在线，并且不重启。
磁盘上若还有 `updates/r51`，先用当时安装留下的证据目录跑回滚，再安装。回滚现清 r51 与 r52 两槽。没有残留就直接安装。不要拿尚未安装的新构建目录去回滚。
证据目录没有 `install-backup/` 时，脚本不会清槽。按升级与回滚节手工清两槽，再 `update-initramfs`。

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
`hwinfo_g0m.bin` 只在 `firmware_en=1` 时被请求，不是必需。请求行根因是自配置 `firmware_en=1`，不是缺文件。包内 `fantgpu.conf` 写 `firmware_en=0`，注释一行、参数一行；modprobe 不剥行尾 `#`。已经装过旧包的机器要等下次安装覆盖这行，再重启才离开 1。背光随这行一起回来。O 血统 `innogpu.conf` 仍是历史的 `firmware_en=1`，本轮不改。封闭对象还可能打出 `hwinfo_get_val` 的 `117 not found` 和 `hwinfo_get_item` 的 `195 not found in memory`。这是查表，不是请求行，先记录。

2026-10-31：Debian `6.12.111+deb13-amd64` 上 `firmware_en=0` 已启动。DKMS 把 `fantgpu.ko` 放进 initramfs，根文件系统挂载前就初始化。值为 0 时封闭对象仍报 hwinfo 完成，`m-030` 挡不住，`fh2m_hal_os_release_match()` 在 initramfs 里读发行文件失败后空指针崩溃。早载来自厂商钩子 `vendor/fantgpu/usr/share/initramfs-tools/hooks/fantgpu`，内容是 `manual_add_modules fantgpu`。`m-052-firmware-en-early-guard` 已入仓库：`firmware_en=0` 时在 `init_os_name()` 里直接返回 `DMA_POINTER_NORMAL`，不挪 m-030 的校验位置。若系统本应匹配 NFSDesktop 或 SeewoOs，模式会是 NORMAL，不是 DMAIRQ 或 HWTIMER。项目默认是 0，目标是 Debian 与 Gentoo，这个差别可接受。封闭对象内部没有逐行复核；结论依据是现场崩溃链，以及同一守卫后 111/107/101 三核启动通过。现场 srcversion `71E8B0D938D845DE8C1B136` 与 `3F4865B566A457EAAB1093B` 不能顶替仓库身份，旧身份 `370AB248D72951EF3C0C4BB` 也不能。Debian 101 的未安装构建身份是 `51EF2B9AEC2C29D01AFE76E`。它不顶替 111 的 `370AB248D72951EF3C0C4BB`，也不顶替正在运行的 `3F4865B566A457EAAB1093B`。请求行全 0，无驱动崩溃，`panel_backlight` 在。另有 1 条 `think_lmi/wmi` 平台 Call Trace，不是本驱动。`117 not found` 六次、`195 not found in memory` 一次仍单列。`GRUB_DEFAULT` 仍是 `Gentoo`。

重装旧 deb 会盖掉已装源码里的现场守卫。新包在派生之后打 `m-036`、`m-051` 和 `m-052`，包内源码含 `!firmware_en`。没换成这个包之前，早载加 `firmware_en=0` 仍可能崩溃。

2026-10-31 已用同一份守卫源码重编 101 与 107，并重建这两核的 initramfs。101 的磁盘模块与 initramfs 都是 `3F4865B566A457EAAB1093B`，107 的两边都是 `71E8B0D938D845DE8C1B136`。

2026-10-31 三核都在 `firmware_en=0` 下重启过：111 boot `e623c081`，107 boot `344f5c63`，101 boot `e74efa78`。三核请求行、`BUG`、`Oops` 都是 0，`panel_backlight` 都在。三核都剩 `117 not found` 六次、`195 not found in memory` 一次。`117` 在 `firmware_en=1` 时已有；当时还有 `207 not found in memory`（R58 启动信息审计记录：117 恰 5、207 恰 1），这次没看到。`195` 不能写成与值为 1 时相同。这些是封闭对象查表缺项，先记录，以后再处理。

Gentoo 没撞上，是因为模块在 `extra/`，根文件系统挂载后才加载，发行文件已经可读。同一守卫已进 Gentoo 构建链，现场模块还没重编。以后若把 Gentoo 模块放进 initramfs，必须先用带 `m-052` 的源码重编。

## 卸载

已装包的 `postrm` 在 remove 和 purge 时只跑 `ldconfig`。
purge 不删除 `/var/lib/dkms/fantgpu-fh2m-kernel`（101、107、111 三份构建都在），
也不删除 `/lib/modules/*/updates/dkms` 里的 fantgpu 模块。

构建器源码的 `prerm` 会尝试 `dkms remove -m fantgpu-fh2m-kernel -v 2.2 --all`，失败被忽略。
现场 purge 之后这些目录仍在，不能把这一行当成已经清掉。

要清就先看 `dkms status`，再 `sudo dkms remove -m fantgpu-fh2m-kernel -v <版本> --all`。
`updates/dkms` 里剩下的 fantgpu 模块删掉后跑 `depmod -a`。不加载的话留着无害。

Gentoo 卸包会删掉登记的 `extra/fantgpu.ko`，`pkg_postrm` 再 `depmod` 和 `ldconfig`；目标 build 目录缺失则跳过 `depmod`，不阻断卸载。没有 DKMS 树。
`/var/lib/fantgpu/rollback/fantgpu.ko` 不在包里，卸载不删。

## 升级与回滚

`/etc/kernel/postinst.d/dkms` 在。安装新的内核包时，已注册的 DKMS 模块会自动重编。
Gentoo 没有这个钩子，要先把 `/usr/src/linux` 指到目标内核，再 `emerge @module-rebuild`。
两边的 ko 不能对拷。不要改 GRUB。

`6.12.101+deb13-amd64` 和 `6.12.107+deb13-amd64` 保留，DKMS 里已有对应构建。
回滚卡仍是安装节的 `4.0.2-i3`，现场只剩配置文件。
Gentoo 回滚只把保存的那一份 ko 写回当前内核的 `extra/`，然后重启。
`tools/debian/rollback-linux-patched-modules.sh` 删除 `updates/r51` 与 `updates/r52` 的 `battery.ko` 和 `xhci-pci.ko`，并恢复保存的 initramfs。本轮未实机验证。
证据目录没有 `install-backup/` 时，脚本停在备份缺失，两槽 ko 还在。手工只删这两槽的 `battery.ko` 和 `xhci-pci.ko`，再 `depmod` 和 `update-initramfs`。这不是脚本回滚：没有保存的 initramfs 可恢复，只是让新 initramfs 不再打包这两槽。目录里还有别的文件就不要删。不要改 GRUB。本轮未实机验证。

```sh
KERNEL_RELEASE=<目标内核>
sudo rm -f \
  "/lib/modules/$KERNEL_RELEASE/updates/r51/battery.ko" \
  "/lib/modules/$KERNEL_RELEASE/updates/r51/xhci-pci.ko" \
  "/lib/modules/$KERNEL_RELEASE/updates/r52/battery.ko" \
  "/lib/modules/$KERNEL_RELEASE/updates/r52/xhci-pci.ko"
sudo rmdir "/lib/modules/$KERNEL_RELEASE/updates/r51" "/lib/modules/$KERNEL_RELEASE/updates/r52" || true
sudo depmod -a "$KERNEL_RELEASE"
sudo update-initramfs -u -k "$KERNEL_RELEASE"
```
