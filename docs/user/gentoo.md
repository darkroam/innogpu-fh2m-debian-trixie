# Gentoo 现场档案

这是 Gentoo 的平行档案，不改 Debian 侧的当前内核结论。Debian 仍以
[status.md](../project/status.md) 为准。

## 这台机器

- 内核 `6.12.111-gentoo-x86_64`。6.18 因封存对象的 `%gs:0x28` 栈金丝雀对不上，已放弃。
- 模块是 i12 构建器树 `9a8d185f…`，再在构建器外打 `030-036`。整树
  `0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce`。
- 2026-10-12 启动 `c285b6cb-efcf-43f4-9f13-5c059c970c16`：srcversion
  `370AB248D72951EF3C0C4BB`，`card0` 在，dwm 在跑，glvnd 选到 `libGLX_fh2m.so.0`，
  渲染器 `Fantasy II-M`。VA-API 打开 `fh2m_drv_video.so`，`va_openDriver()` 返回 0。
  八项计数为 0。oops 为 0。签名 taint 是 `OE`。
- `hwinfo_g0m.bin` 仍缺。这不是通过。
- 热卸载会在 `hwinfo_unregister` 里 oops。换模块只改磁盘上的 ko，然后重启。不要 unbind。

## 安装

正式入口是本仓库里的 portage overlay：`tools/gentoo-overlay`。

```
# /etc/portage/repos.conf/fantgpu.conf
[fantgpu]
location = /path/to/innogpu-fh2m-debian-trixie/tools/gentoo-overlay
auto-sync = no
```

`location` 必须让 `portage` 用户能读。家目录是 `700` 时不要指到家目录里。
Portage 每次下载源码都会以这个用户读每个 overlay 的 `profiles/thirdpartymirrors`。
这个文件可以没有，没有就当空表。进不了目录则是权限错误，整次 emerge 失败，
连无关的包也装不上。本机因此把 overlay 拷到 `/var/db/repos/fantgpu` 再登记。
`tools/gentoo-overlay` 仍是源头，改完要再拷一次。

然后接受 `~amd64` 和 `fantgpu-vendor`。驱动 ebuild 调用 `tools/gentoo-fh2m.sh`。

```
# /etc/portage/package.accept_keywords/fantgpu-fh2m
x11-drivers/fantgpu-fh2m ~amd64
sys-kernel/fantgpu-kernel-patches ~amd64

# /etc/portage/package.license/fantgpu-fh2m
x11-drivers/fantgpu-fh2m fantgpu-vendor
```

一条安装顺序：

```
emerge sys-kernel/fantgpu-kernel-patches
emerge -u sys-kernel/gentoo-sources
# 用现有内核构建方式编译并安装，然后重启。不要改 GRUB。
emerge x11-drivers/fantgpu-fh2m
# 再重启
tools/gentoo-fh2m.sh verify
tools/gentoo-check-kernel-patches.sh
```

用户态和固件来自本机已经物化的 `vendor/fantgpu/`，不重新打包，也不进 git。
`RESTRICT=bindist`。装完要重启。emerge 不会热卸载正在用的模块。

没有 portage 时用同一脚本：

```
tools/gentoo-fh2m.sh check
tools/gentoo-fh2m.sh build
sudo tools/gentoo-fh2m.sh install
sudo tools/gentoo-fh2m.sh verify
```

`install` 会先把当前 ko 存到 `.build/work/gentoo-fh2m/rollback/fantgpu.ko`。
退回用 `sudo tools/gentoo-fh2m.sh rollback`，然后重启。

构建器 `tools/build-innogpu-driver.sh` 仍只产 Debian deb，并且停在 `030-035`。
Gentoo 脚本只借用它的 `derive_fantgpu_source`，再打 `030-036`。不改构建器。

## 不在本档案里

- `patches/030-037` 到 `030-050` 不入库。
- 不跑 PM，不改 GRUB。
- initramfs 不必重建。模块和固件都在根文件系统挂载之后加载。
- Xorg 用 21.1 即可。这台机器后来装了 mesa 26，glvnd 仍选 fh2m，因为
  `00_fh2m.json` 排在 mesa 前面，并且 `/etc/ld.so.conf.d/0-fantgpu-hwgl.conf`
  指向 `/opt/fantgpu-fh2m/lib`。

## 依赖

`x11-drivers/fantgpu-fh2m` 的 `RDEPEND` 来自 2026-10-13 对已安装 amd64 载荷的 `readelf -d` `NEEDED` 盘点，外加加载这些插件的包。`libva` 不在 `NEEDED` 里，它是打开 `fh2m_drv_video.so` 的加载器。`libgbm.so.1` 用包内那份，不另拉系统 gbm。`IUSE=+wayland` 默认打开；关掉它不会改已经链接好的 `libwayland-client.so.0` 和 `libwayland-server.so.0`。

- `>=x11-base/xorg-server-21.1.20:0 <x11-base/xorg-server-22:0`。本机 slot 是 `0/21.1.25`，不能写成 `:0/21.1`。
- `media-libs/libva:0/2`、`media-libs/libglvnd`、`media-libs/libepoxy`
- `x11-libs/libdrm`、`libX11`、`libXext`、`libXfixes`、`libXxf86vm`、`libxcb`、`libxshmfence`、`pixman`
- `dev-libs/expat`、`sys-libs/zlib`、`virtual/libudev`、`sys-libs/glibc`、`sys-devel/gcc`
- `wayland? ( dev-libs/wayland )`，这一个包同时提供 client 和 server
- 构建依赖 `virtual/linux-sources`

`sys-kernel/fantgpu-kernel-patches` 自己不拉这些包，也不拉内核源码。

## 内核补丁 001–004

权威文件在 `tools/gentoo-overlay/sys-kernel/patches/`，字节与 `components/linux` 的同名补丁一致。
001/002 的基线仍是 Debian `linux-source-6.12` `6.12.107-1`，换内核必须重验。
003 只给 `pinctrl-amd` 增加 ACPI ID `HYGO0030`。004 只给 6.12 的 `snd-hda-intel` 增加
PCI `1d94:14c9`。004 不能套到 6.18：控制器源码已改到 `sound/hda/controllers/intel.c`，
而且兆芯条目和龙芯条目之间多了 HDMI ID。

正常路径是 `emerge sys-kernel/fantgpu-kernel-patches`。`pkg_postinst` 把四份补丁拷进
`/etc/portage/patches/sys-kernel/gentoo-sources/` 和 `gentoo-kernel/`。SHA 相同就跳过。
SHA 不同就失败，不覆盖已经改过的本地文件。没有 portage 时用同一规则的后备：

```
sudo tools/gentoo-install-kernel-patches.sh
```

这不改正在运行的内核。已经解包的树如果被手工改过，
先用四个补丁 `--fuzz=0 -R` 卸回，再从
`/etc/portage/patches/sys-kernel/gentoo-sources/` 按文件名顺序 `--fuzz=0` 打回。
现在不按内核版本拆目录。`emerge` 任何 gentoo-sources 都会尝试打这组补丁；6.18 的声卡补丁对不上时要停下来重做，不要带着失败继续编。

```
emerge -u sys-kernel/gentoo-sources
# 用现有内核构建方式编译并安装，然后重启。不要改 GRUB。
tools/gentoo-check-kernel-patches.sh
```

check 要同时看到 xHCI Hygon quirk、KaiTian 电池条目、`HYGO0030` 和 `1d94:14c9` 才返回 0。
003 是内建项（`CONFIG_PINCTRL_AMD=y`），源码重打后必须整核重编并重启才进正在运行的内核。
004 是模块（`CONFIG_SND_HDA_INTEL=m`），可以只编译 `sound/pci/hda` 再热加载。
触摸板还需要 `CONFIG_I2C_HID_ACPI=m` 和 `CONFIG_HID_MULTITOUCH=m`。

## 一轮 deep

一轮采集用自动唤醒，不写 `pm_test`。屏幕会灭，到时自己醒来。默认 45 秒，可改成 20 到 180：

```
tools/gentoo-r5-cycle.sh
tools/gentoo-r5-cycle.sh 60
```

它先跑 `gentoo-r5-observe.sh before`，再用 `rtcwake -m mem` 休眠并唤醒，然后跑 `after`。
手点休眠仍可用 `before` / `systemctl suspend` / `after`。

普通用户启动，脚本自己要求 sudo，并把提权记进 `.build/r57-r5-observe/launcher.log`。
`latest-run.txt` 指向本次目录。温度监控和 `recover gpu pll` 没有新增时，结果是
`RECORDED_OBSERVATION_ABSENT`，不是通过。正在运行的 xHCI 若仍是 initramfs 里的旧模块（quirks 不是 `0x90`），外接键盘只作记录。
电池、GPIO、板载声卡在对应补丁进入正在运行的镜像之前，也不要把记录写成验收。
2026-10-12 18:28 的自动一轮发生在 quirks=`0x90` 之后：同一启动，驱动恢复 rc=0，观察行仍没有，不是通过。外接键盘没有消失，但该口有一条 context 警告。提前约 24 秒醒来，来源未拆。
