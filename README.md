# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上的 Innosilicon Fantasy II-M / 风华2号M（innogpu fh2m）驱动打包与兼容修复。

## 当前状态

| 阶段 | 状态 |
|------|------|
| 内核模块编译 (DKMS) | ✓ patched-6/7/8/9 均通过 |
| 内核模块加载 | ✓ 自动加载正常，Driver OK，Firmware OK |
| /dev/dri 设备 | ✓ card0 + renderD128 |
| Xorg modesetting + llvmpipe | ✓ 安全回退，随时可用 |
| Xorg innogpu DDX | ✓ 重启验证通过，EDID/显示/输入正常 |
| 硬件 GLAMOR 加速 | ✗ GBM backend 不兼容 |
| 当前 GL | Mesa llvmpipe 软件渲染 |
| 硬件 OpenGL/EGL/Vulkan | 待 GBM backend 解决 |

## 核心阻塞问题

**GLAMOR 硬件加速**需要 vendor GBM backend (`innogpu_gbm.so`)，但当前 Deepin 202504 的 GBM backend 与 Debian 6.12 内核 DRM 栈不兼容：

| 尝试 | 结果 |
|------|------|
| 系统 libgbm + vendor GBM backend | segfault in `gbm_create_device` |
| vendor libgbm (LD_PRELOAD) + vendor GBM backend | 同样 segfault |
| 禁用 GBM backend（当前方案） | DDX 正常工作，GL 回退到 llvmpipe |

GBM backend 崩溃发生在 `gbm_create_device` 深层调用中，与 Debian 6.12 DRM ioctl/ABI 变化有关。

## 等效命令：安装硬件 DDX（当前推荐）

```bash
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-9.deb
sudo modprobe innogpu
sudo innogpu-install-kylin-userspace /home/ok/src/innogpu-fh2m-deepin-202504/root
sudo innogpu-test-xorg-once   # 验证 Xorg 不崩溃
sudo reboot
```

重启后 Xorg 自动使用 innogpu DDX（EDID、显示、输入正常），GL 由 Mesa llvmpipe 提供。

**恢复安全 modesetting：**
```bash
sudo innogpu-disable-incompatible-userspace && sudo reboot
```

## 已尝试的用户态来源

### 1. 麒麟 V10 系统盘 (`/mnt/kylin-root`)

- 包名：`innogpu-fh2m`，版本 `3.2.1.16-v10.2-kylin`
- DDX 为 Xorg video ABI 24 (Xorg 1.20.4)，Debian Trixie ABI 25 不兼容
- 源码线索：`G0M_DDK_V119RTM_RELEASE_BUILD_PIPELINE_DDK`

### 2. 麒麟 V11 Server ISO

- `~/downloads/Kylin-Server-V11-2503-Release-General-20250715-X86_64.iso`
- RPM 系统，无 innogpu-fh2m 包

### 3. Kylin 在线源

- `archive.kylinos.cn/kylin/KYLIN-ALL` — 仅 `3.2.1.16-v10.2-kylin`（同 #1）
- `archive2.kylinos.cn/DEB/KYLIN_DEB` V11 — 无 innogpu
- `updates.kylinos.cn/NS/V11/2503` — chroot dnf search 无结果

### 4. Deepin V23 / beige 社区源 ✓ 最佳候选

```
https://community-packages.deepin.com/deepin/beige/pool/commercial/i/innogpu-fh2m/
  innogpu-fh2m_20250421190503-debug_amd64.deb  78 MB
  innogpu-fh2m_202504211717-debug_loong64.deb   33 MB
  innogpu-fh2m_20250421190503-debug_arm64.deb   29 MB
```

AMD64 SHA256: `b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`

本地：`/home/ok/src/innogpu-fh2m_20250421190503-debug_amd64.deb`
解包：`/home/ok/src/innogpu-fh2m-deepin-202504/`

关键优势：
- DDX: Xorg video ABI 25.2 ✓
- DRI: Mesa 23.1.3
- 包含 swrast_inno_dri.so, innogpu_drv_video.so, innogpu_gbm.so
- DKMS 内核源码已更新（~50个文件与 patched-6 基准不同）

### 5. Linglong 仓库

GitHub: `linglongdev/cn.innosilicon.driver.innogpu.fh2m`
引用的 deb 与 #4 相同。

### 6. 麒麟系统盘备份

`/home/ok/src/innogpu-kylin-userspace-backup/` （66 MB tar.gz）
含麒麟 V10 完整 innogpu 用户态栈、dpkg metadata、Xorg 日志。

## 备份文件

| 文件 | 大小 | 说明 |
|------|------|------|
| `/home/ok/src/innogpu-kylin-userspace-backup/` | - | 麒麟 V10 用户态备份目录 |
| `/home/ok/src/innogpu-kylin-userspace-backup.tar.gz` | 66 MB | 麒麟 V10 用户态打包 |
| `/home/ok/src/innogpu-fh2m_20250421190503-debug_amd64.deb` | 78 MB | Deepin 202504 deb |
| `/home/ok/src/innogpu-fh2m-deepin-202504/` | - | Deepin deb 解包目录 |
| `/home/ok/readme-gpu` | - | 首次调查记录 |

## 仓库文件

```
patches/001-kernel-6.12-compat.patch      — kernel 6.12 兼容补丁
scripts/install.sh                        — 从官方包安装的旧流程
scripts/build-patched6.sh                 — 打包 patched-6（PLL workaround）
scripts/build-patched7.sh                 — 打包 patched-7（用户态安装器）
scripts/build-patched9.sh                 — 打包 patched-9（当前推荐）
scripts/patch-skip-first-gpupll.sh       — PLL workaround
scripts/disable-incompatible-userspace.sh — 恢复安全 modesetting
scripts/install-kylin-userspace.sh        — 从麒麟/UOS/Deepin root 安装用户态（跳过 GBM backend）
scripts/test-xorg-once.sh                — 非重启 Xorg 测试
*.deb                                     — Release 包
```

## 下一步

1. 寻找 GBM backend 的修复或更新版（新 Deepin/UOS 包可能有兼容 Debian 6.12 的版本）
2. 或获取 GBM backend 源码后在 Debian 6.12 上重新编译
3. GBM backend 解决后再测试 GLAMOR 硬件加速、eglinfo、Vulkan
