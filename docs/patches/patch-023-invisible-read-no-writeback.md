# Patch 023：invisible GEM 只读映射释放不回写

## 目的

FH2M 的 invisible VRAM 通过每页 staging buffer 支持 CPU mmap。page fault 先执行 `GDDR2SYS`，但
VMA close 不区分 CPU_PREP 的 READ/WRITE，始终对每页执行 `SYS2GDDR`。只读 DMA-BUF surface 因此
在 `munmap` 时产生与页数成比例的无意义回写。

独立 `tools/probe-pdp-invisible-read.c` 已在当时运行中的 patched-22 上复现：7,646,720 字节、1867 页的 READ
mapping 在逐页读取后，三轮 `munmap` 分别消耗 71.915、119.357 和 96.716ms system CPU。该复现不
依赖 WebKit、GBM、EGL 或图形桌面。

## 实现

- 代码：`patches/023-invisible-read-no-writeback.patch`。
- 构建开关：`APPLY_INVISIBLE_READ_NO_WRITEBACK=1`。
- 固定候选：`scripts/build-patched23-invisible-read-fix.sh`，版本 `3.3.3.42-patched-23`。
- CPU_PREP 成功后记录当前访问是否允许 WRITE；没有显式 READ 的异常/旧调用保守视为 WRITE。
- invisible page fault 把访问方向固化到 staging 页；CPU_FINI 后不再依赖对象的瞬时状态。
- READ 页在 VMA close 时直接释放；WRITE 页保留原有 `SYS2GDDR`。
- 已存在的 staging 页遇到后续 WRITE CPU_PREP 时升级为需要回写，避免同一 mmap 先读后写时丢数据。

该补丁不修改 `dma_resv` fence usage、vblank、GEM 分配位置或 Deepin 用户态库，也不重建任何
历史版本。

## 离线验证门槛

1. 补丁必须能在 Deepin 202504 原 deb 解包源码上、现有 p22 补丁之后无 fuzz 应用。
2. DKMS 必须针对当前 Debian 6.12 headers 编译成功。
3. release 包边界、Deepin DRI/GBM/固件逐字一致性和文档检查必须通过。
4. 最小探针必须以 `-Wall -Wextra -Werror` 编译，并同时支持 READ 性能测试与 WRITE 后读回验证。

候选包生成后使用以下只读包/临时目录检查，不注册 DKMS、不安装模块：

```sh
scripts/check-deb-dkms-build.sh \
  debs/innogpu-fh2m-trixie_3.3.3.42-patched-23.deb
```

## 当前离线结果

- patched-23 已从 Deepin 202504 原 deb 构建、安装并在重启后验证；包边界和同源用户态/固件检查通过；SHA-256 为
  `da1479f6264406443616f342e917b7f95d5798c98b1874c5b5abed38a9012715`。
- 使用固定 `SOURCE_DATE_EPOCH=1786924800` 连续构建两次，SHA-256 完全一致。
- 包内 DKMS 源码已针对 `6.12.96+deb13-amd64` headers 编译成功，`innogpu.ko` vermagic 匹配。
- p22 与 p23 使用同一离线检查器均产生 4421 条 Deepin 厂商源码既有 warning，p23 未增加 warning。
- 新探针以 `-Wall -Wextra -Werror` 编译通过。p22 的一轮 READ 基线为 touch 111.387ms system、
  `munmap` 109.800ms system；WRITE 后逐页 READ 验证通过。

上述离线门槛和一次重启后的基础实机验证均已通过。Clash Verge 已完成启动态 A/B：禁用
DMA-BUF 时主进程约 1.00% CPU，直接启用 DMA-BUF 时约 13.6%；本次短时空闲采样未复现历史
2860% 忙等，但仍显示 DMA-BUF 路径存在额外系统 CPU 风险。启动包装脚本继续设置
`WEBKIT_DISABLE_DMABUF_RENDERER=1`。

## 安装前检查与恢复

当前设备已核对的本地包为：

| 包 | SHA-256 | 用途 |
| --- | --- | --- |
| patched-21 | `15c1fab4b8a0f36985097e3d1651ff43fc09f00a2bda47058c380e1e561384cc` | 完整图形验收回退点 |
| patched-22 | `aae8f966af7c5737037869a4e6ee5d081fd07d386dec67e0e799746ff6386ae9` | 直接回退点 |
| patched-23 | `da1479f6264406443616f342e917b7f95d5798c98b1874c5b5abed38a9012715` | 已安装并完成基础图形及 Clash 启动态 A/B |

安装前先确认三个文件仍匹配上述哈希。若 patched-23 重启后黑屏、PVR 异常或无法进入桌面，通过
TTY/SSH 直接恢复 patched-22：

```sh
sudo dpkg -i debs/innogpu-fh2m-trixie_3.3.3.42-patched-22.deb
sudo reboot
```

若 patched-22 也不能恢复完整图形路径，再恢复 patched-21：

```sh
sudo dpkg -i debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
sudo reboot
```

部署 patched-23 时只执行正常包安装，不卸载或热切换活动模块。安装完成后先运行不带
`--require-reboot` 的状态检查，确认包和 DKMS 已落盘，再做一次激活重启：

```sh
sudo dpkg -i debs/innogpu-fh2m-trixie_3.3.3.42-patched-23.deb
scripts/verify-install-status.sh 3.3.3.42-patched-23
sudo reboot
```

## 部署后验收门槛

1. READ touch 允许保留必要的 `GDDR2SYS` 成本，READ `munmap` system CPU 必须显著低于 p22 基线。
2. WRITE 模式写入每页后解除映射，再以 READ mapping 读回，数据必须完全一致。
3. Clash Verge A/B 已完成；禁用 DMA-BUF 的包装脚本作为当前设备的稳定运行方式保留。
4. 重新执行 PVR、DRM/fbdev、Xorg/GLX、真实 VT、xdisplay、Picom 和音频回归。

## 回退

patched-23 运行验证失败时恢复当前已保留的 patched-22；若图形完整回归失败，恢复已完整验收的
patched-21。p23 已完成安装、更新 initramfs、重启和基础图形验证。
