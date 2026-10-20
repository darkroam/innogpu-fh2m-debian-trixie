# Gentoo 现场档案

这是 Gentoo 的平行档案，不改 Debian 侧的当前内核结论。Debian 仍以
[status.md](../project/status.md) 为准。

## 这台机器

- 内核 `6.12.111-gentoo-x86_64`。6.18 因封存对象的 `%gs:0x28` 栈金丝雀对不上，已放弃。
- 模块基座是 i12 构建器树 `9a8d185f…`。构建器外先打 `m-036-bf-shf-rename`，该层整树
  `0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce`，再打
  `m-051-hwinfo-connector-fallback`。m-051 不进 i12 构建器。
- 2026-10-12 启动 `c285b6cb-efcf-43f4-9f13-5c059c970c16`：当时 srcversion
  `370AB248D72951EF3C0C4BB`，`card0` 在，dwm 在跑，glvnd 选到 `libGLX_fh2m.so.0`，
  渲染器 `Fantasy II-M`。VA-API 打开 `fh2m_drv_video.so`，`va_openDriver()` 返回 0。
  八项计数为 0。oops 为 0。签名 taint 是 `OE`。
- 现行加载 srcversion 是 `D579486A64C340B5178B66B`（2026-10-15 本机实读）。这不是 Debian 111 的 `370AB248D72951EF3C0C4BB`。
- 磁盘 ko `f888e32ecaed`（66606896 字节）与工作树 `.build/work/gentoo-fh2m/fantgpu.ko` 的 `ea8c6c60d113`（66606760 字节）不同。两者 srcversion 都是上面这一条。构建门是同一棵 m-051 树 `b0a46c345c1e0d93baf51c554a1cc9dde0757feb68ce4a9951c3c01458f71c00`；本机没有重走两份源树。`.text`、`.data`、`.rodata`、`.modinfo` 逐节相同。整文件多出的 136 字节都在 `.rodata.str1.8`：路径长度差 125，对齐填充 11。磁盘模块嵌 `/var/tmp/portage/x11-drivers/fantgpu-fh2m-5.0.0_p12/temp/gentoo-fh2m/src/`，工作树模块嵌 `.build/work/gentoo-fh2m/src/`。这是构建目录嵌入，不是换源。
- `hwinfo_g0m.bin` 仍缺。这不是通过。
- 热卸载会在 `hwinfo_unregister` 里 oops。换模块只改磁盘上的 ko，然后重启。不要 unbind。

## 安装

正式入口是本仓库里的 portage overlay：`tools/gentoo/overlay`。

```
# /etc/portage/repos.conf/fantgpu.conf
[fantgpu]
location = /path/to/innogpu-fh2m-debian-trixie/tools/gentoo/overlay
auto-sync = no
```

`location` 必须让 `portage` 用户能读。家目录是 `700` 时不要指到家目录里。
Portage 每次下载源码都会以这个用户读每个 overlay 的 `profiles/thirdpartymirrors`。
这个文件可以没有，没有就当空表。进不了目录则是权限错误，整次 emerge 失败，
连无关的包也装不上。本机因此把 overlay 拷到 `/var/db/repos/fantgpu` 再登记。
`tools/gentoo/overlay` 仍是源头。部署时整目录同步到 `/var/db/repos/fantgpu`，
再写一行 `repo-root`，内容是这个 git 仓库的根。家目录是 `700`，portage 用户进不去，
所以 `x11-drivers/fantgpu-fh2m` 还要有一条 package.env：`FEATURES="-userpriv"`。
编译阶段因此以 root 读仓库。不要改家目录权限。

```
rsync -a --delete tools/gentoo/overlay/ /var/db/repos/fantgpu/
printf '%s\n' "$PWD" > /var/db/repos/fantgpu/repo-root
```

然后接受 `~amd64` 和 `fantgpu-vendor`。驱动 ebuild 调用 `tools/gentoo/build-driver.sh` 和 `tools/gentoo/install-driver.sh`。

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
tools/gentoo/verify.sh
tools/gentoo/check-kernel-patches.sh
```

用户态和固件来自本机已经物化的 `vendor/fantgpu/`，不重新打包，也不进 git。
`RESTRICT=bindist`。装完要重启。emerge 不会热卸载正在用的模块。

没有 portage 时用同一脚本：

```
tools/gentoo/check.sh
tools/gentoo/build-driver.sh
sudo tools/gentoo/install-driver.sh
sudo tools/gentoo/verify.sh
```

`install` 会先把当前 ko 存到 `.build/work/gentoo-fh2m/rollback/fantgpu.ko`。
退回用 `sudo tools/gentoo/rollback.sh`，然后重启。

构建器 `tools/debian/build-driver.sh` 仍只产 Debian deb，并且停在 `m-035-private-accessor`。
Gentoo 与构建器共同 source `tools/internal/derive-fantgpu-source.sh`，再打 `m-036-bf-shf-rename` 和 `m-051-hwinfo-connector-fallback`。不改构建器。

## 升级内核

目标内核是 `/usr/src/linux`（或 `KERNEL_DIR`）指向的源码，不是正在运行的内核。多内核先 `eselect kernel`。版本用这份树的 `include/config/kernel.release`，也就是 `linux-info` 的 `KV_FULL`（含 localversion）。`/lib/modules/<KV_FULL>/build` 不存在就停，不退回正在运行的内核。

`001`–`006` 仍由 portage user-patches 打进内核源码。`fantgpu.ko` 不随内核包安装，要另编进该版本的 `extra/`。本包已经拥有 `extra/fantgpu.ko`，所以 `emerge @module-rebuild` 能列出它，不必再做 `MODULESD_*` 注册。genkernel 4.3.17 没有 `post.d`，内核装完后不会自动重编模块。ebuild 不 strip、不压缩、不签名。

重编写入当前 `KV_FULL` 的 `extra/`。两颗内核的 ko 不能对拷。不要改 GRUB。不要热卸载。

```
emerge sys-kernel/fantgpu-kernel-patches
# /usr/src/linux 指向目标内核
emerge -u sys-kernel/gentoo-sources
# 用现有方式编译并安装内核，然后重启。不要改 GRUB。
emerge @module-rebuild
# 再重启
tools/gentoo/check-kernel-patches.sh
tools/gentoo/verify.sh
```

`check-kernel-patches.sh` 不加 `--files-only` 时，还要求该 `kernel.release` 的 `extra/fantgpu.ko` 存在，没有就打印 `module_file=MISSING` 并失败。`verify` 先看这份 ko，缺了报 `module file missing`，不会先报 `no gpu status`。

## 幂等

`build` 重跑应得到同一份 `fantgpu.ko`。`install` 只在回滚件不存在时保存当前磁盘 ko。
emerge 的回滚件在 `/var/lib/fantgpu/rollback/fantgpu.ko`，不放在 portage 的临时目录里。
第二次安装看到这个文件就跳过，日志是 `rollback_kept`。不要热卸载。换上去的 ko 在重启后才是正在运行的那份。

## 卸载

`emerge --unmerge x11-drivers/fantgpu-fh2m` 会删掉包里登记的文件。模块如果还在 `lsmod` 里，
`pkg_prerm` 只警告：模块已加载，不要热卸载，先重启，再把这次卸载当成完成。`pkg_postrm` 跑
`depmod` 和 `ldconfig`，不执行 `rmmod`。

手动入口没有卸载。它只做 check、build、install、verify、rollback。
`rollback` 把保存的 ko 写回磁盘，然后重启，同样不热卸载。没有 portage 时，补丁拷贝用
`tools/gentoo/install-kernel-patches.sh`，它也不卸载。

2026-10-13 本机已经 `emerge` 了 `sys-kernel/fantgpu-kernel-patches` 和 `x11-drivers/fantgpu-fh2m`。
补丁包因为目标 SHA 相同，`installed=0 skipped=8`。驱动第一次装到已有手动文件上时，portage 报文件冲突；
这些文件原来不属于任何包，合并后归这个包。没有 `rmmod`。磁盘上的 ko 要重启后才是正在运行的那份。
回滚件在 `/var/lib/fantgpu/rollback/fantgpu.ko`。同一目录再编两次，ko 字节相同；换工作目录只会把路径编进调试信息。

2026-10-13 对已安装 amd64 ELF 重跑 `readelf -d`。外部 `NEEDED` 都在 `RDEPEND` 里。`libgbm.so.1` 解析到包内，`libEGL.so.1` 解析到 `libglvnd`。因此已从 `@world` 去掉 `dev-libs/wayland`、`media-libs/libva`、`x11-base/xorg-server`、`media-libs/mesa`。不要 `depclean` 之前先看下面两条仍须留在 `package.use` 的项。不要开全局 `USE=wayland`。

仍须手写，ebuild 没有表达：

- `x11-base/xorg-server -minimal`。`minimal` 关掉 dri2、dri3、glamor、glx。DDX 用 `DRI2*` 和 `dri3_screen_init`。
- `x11-base/xorg-drivers` 与 `media-libs/mesa` 的 `-video_cards_*`，以及 mesa 的 `-llvm`。这是这台机器不编其他显卡驱动，不是漏掉的包。`xorg-server` 会拉这两份包。

2026-10-19 对 `/opt/fantgpu-fh2m` 下 22 个 amd64 ELF 重跑 `readelf -d`。没有一条 `NEEDED` 是 `libvulkan.so` 或 `libOpenCL.so`。`libVK_FANT_fh2m.so.1` 和 `libFTOCL_fh2m.so.1` 是 ICD。打开它们的 loader 照 `libva` 的规则写进 `RDEPEND`：`media-libs/vulkan-loader` 读 `/usr/share/vulkan/icd.d/fh2m_conf.json`，`virtual/opencl` 拉 Khronos ICD loader（`libOpenCL.so`）读 `/etc/OpenCL/vendors/FANT_fh2m.icd`。这不是再装一套 mesa/pocl 硬件运行时。改完 ebuild 之后要重新 `emerge` 驱动包才会装上 loader；本段没有执行 emerge。

## 不在本档案里

- `g-037` 到 `g-050` 已按命名规范入库。入库不等于升格：仍标 g，字节不变，不进 i12 构建器。`g-050-fb-helper-alloc` 是 6.12 胶水，不是 6.18。
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
- `media-libs/vulkan-loader`、`virtual/opencl`。不是 ELF `NEEDED`，是打开 Vulkan/OpenCL ICD 的 loader
- 构建依赖 `virtual/linux-sources`

`sys-kernel/fantgpu-kernel-patches` 自己不拉这些包，也不拉内核源码。

## 内核补丁 001–006

权威文件在 `tools/gentoo/overlay/sys-kernel/patches/`，字节与 `components/linux` 的同名补丁一致。
005 只给隐藏项 `CONFIG_DRM_DISPLAY_HDMI_HELPER` 加菜单提示，不改变默认值。
006 只给隐藏项 `CONFIG_VMAP_PFN` 加菜单提示 `Vmap an array of PFNs`，不改变默认值。目标文件是 GPL-2.0-only。Gentoo current 含 005 与 006；Debian current 仍是 001–004。
2026-10-20 三证后 Gentoo current 收入 005 与 006：check 为 PASS / PASS / APPLIED，运行内核两项均为 `=y`，加载模块 srcversion `D579486A64C340B5178B66B`。2026-10-19 的 `#8` 启动本身不是这次验收。
001/002 的基线仍是 Debian `linux-source-6.12` `6.12.107-1`，换内核必须重验。
003 只给 `pinctrl-amd` 增加 ACPI ID `HYGO0030`。004 只给 6.12 的 `snd-hda-intel` 增加
PCI `1d94:14c9`。004 不能套到 6.18：控制器源码已改到 `sound/hda/controllers/intel.c`，
而且兆芯条目和龙芯条目之间多了 HDMI ID。

正常路径是 `emerge sys-kernel/fantgpu-kernel-patches`。`pkg_postinst` 把六份补丁拷进
`/etc/portage/patches/sys-kernel/gentoo-sources/` 和 `gentoo-kernel/`。SHA 相同就跳过。
SHA 不同就失败，不覆盖已经改过的本地文件。没有 portage 时用同一规则的后备：

```
sudo tools/gentoo/install-kernel-patches.sh
```

这不改正在运行的内核。已经解包的树如果被手工改过，
先用这组补丁 `--fuzz=0 -R` 卸回，再从
`/etc/portage/patches/sys-kernel/gentoo-sources/` 按文件名顺序 `--fuzz=0` 打回。
现在不按内核版本拆目录。`emerge` 任何 gentoo-sources 都会尝试打这组补丁；6.18 的声卡补丁对不上时要停下来重做，不要带着失败继续编。

```
emerge -u sys-kernel/gentoo-sources
# 用现有内核构建方式编译并安装，然后重启。不要改 GRUB。
tools/gentoo/check-kernel-patches.sh
```

check 要同时看到 xHCI Hygon quirk、KaiTian 电池条目、`HYGO0030`、`1d94:14c9`、HDMI helper 提示和 `Vmap an array of PFNs` 才返回 0。
005 没打进当前源码树时，这项检查是 `kernel_tree=NOT_APPLIED`。
003 是内建项（`CONFIG_PINCTRL_AMD=y`），源码重打后必须整核重编并重启才进正在运行的内核。
004 是模块（`CONFIG_SND_HDA_INTEL=m`），可以只编译 `sound/pci/hda` 再热加载。
触摸板还需要 `CONFIG_I2C_HID_ACPI=m` 和 `CONFIG_HID_MULTITOUCH=m`。

## 一轮 deep

一轮采集用自动唤醒，不写 `pm_test`。屏幕会灭，到时自己醒来。默认 45 秒，可改成 20 到 180：

```
tools/gentoo/r5-cycle.sh
tools/gentoo/r5-cycle.sh 60
```

它先跑 `tools/gentoo/r5-observe.sh before`，再用 `rtcwake -m mem` 休眠并唤醒，然后跑 `after`。
手点休眠仍可用 `before` / `systemctl suspend` / `after`。

普通用户启动，脚本自己要求 sudo，并把提权记进 `.build/r57-r5-observe/launcher.log`。
`latest-run.txt` 指向本次目录。温度监控和 `recover gpu pll` 没有新增时，结果是
`RECORDED_OBSERVATION_ABSENT`，不是通过。正在运行的 xHCI 若仍是 initramfs 里的旧模块（quirks 不是 `0x90`），外接键盘只作记录。
电池、GPIO、板载声卡在对应补丁进入正在运行的镜像之前，也不要把记录写成验收。
2026-10-12 18:28 的自动一轮发生在 quirks=`0x90` 之后：同一启动，驱动恢复 rc=0，观察行仍没有，不是通过。外接键盘没有消失，但该口有一条 context 警告。提前约 24 秒醒来，来源未拆。
