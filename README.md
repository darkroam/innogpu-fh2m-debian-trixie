# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上的 Innosilicon Fantasy II-M / 风华2号M（innogpu fh2m）驱动打包与兼容修复。

目标：内核 DKMS 模块 + Xorg DDX 硬件加速。

## 当前状态

| 阶段 | 状态 |
|------|------|
| 内核模块编译 (DKMS) | ✓ patched-6/7/8 均通过 |
| 内核模块加载 | ✓ modprobe 成功，Driver OK，Firmware OK |
| /dev/dri 设备 | ✓ card0 + renderD128 |
| Xorg modesetting + llvmpipe | ✓ 安全回退 |
| Xorg innogpu DDX | 进展中（详见下文） |
| 硬件 GL 加速 | 待完成 |

## 已尝试的用户态来源

### 1. 麒麟 V10 系统盘 (`/mnt/kylin-root`)

已挂载同型号机器的麒麟操作系统硬盘。

- 包名：`innogpu-fh2m`，版本 `3.2.1.16-v10.2-kylin`
- 包含完整用户态栈：DRI、EGL、GLX、Vulkan、OpenCL、Xorg DDX
- **结果：Xorg DDX 崩溃** — 该 DDX 为 Xorg video ABI 24 (Xorg 1.20.4) 编译，Debian Trixie 是 ABI 25，即使 `IgnoreABI` 仍然 segfault
- 二进制路径线索：
  - `G0M_DDK_V119RTM_RELEASE_BUILD_PIPELINE_DDK`
  - `../source/include/xorg-1.20.4/privates.h`

### 2. 麒麟 V11 Server ISO

- 文件：`~/downloads/Kylin-Server-V11-2503-Release-General-20250715-X86_64.iso`
- RPM 系统，无 innogpu-fh2m 包
- chroot 搜索在线源也无结果

### 3. Kylin 在线源

- `archive.kylinos.cn/kylin/KYLIN-ALL` — 仅有 `innogpu-fh2m_3.2.1.16-v10.2-kylin_amd64.deb`（同 #1）
- `archive2.kylinos.cn/DEB/KYLIN_DEB` V11 — 无 innogpu
- `updates.kylinos.cn/NS/V11/2503` — chroot dnf search 无结果

### 4. Deepin V23 / beige 社区源 ✓ 最佳候选

URL：
```
https://community-packages.deepin.com/deepin/beige/pool/commercial/i/innogpu-fh2m/
```

AMD64 包：
```
innogpu-fh2m_20250421190503-debug_amd64.deb  (78 MB)
SHA256: b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2
```

本地路径：`/home/ok/src/innogpu-fh2m_20250421190503-debug_amd64.deb`

解包位置：`/home/ok/src/innogpu-fh2m-deepin-202504/`

**关键差异**：
- DDX: Xorg video ABI 25.2，匹配 Debian Trixie ✓
- DRI: Mesa 23.1.3（vs 麒麟版 Mesa 22.1.3）
- 完整用户态库，包括 swrast_inno_dri.so, innogpu_drv_video.so, innogpu_gbm.so
- DKMS 内核源码已更新（~50个文件与 patched-6 基准不同）

**Xorg 测试**：Deepin DDX 可在去掉 vendor GBM backend 后正常启动（无 segfault）

### 5. Linglong 仓库

GitHub: `linglongdev/cn.innosilicon.driver.innogpu.fh2m`
引用的 deb 与 #4 相同。

## 非重启 Xorg 测试进展

对 Deepin 202504 DDX 的测试结果：

| 测试 | 结果 |
|------|------|
| 完整安装含 GBM backend | segfault in `innogpu_gbm.so` → `gbm_create_device` |
| 去掉 `innogpu_gbm.so` | segfault in `libEGL_inno.so` → `gbm_device_get_fd` |
| 去掉 GBM backend + 去掉 EGL vendor allowlist | **Xorg 启动成功！** SWRAST GL 回退 |
| 禁用 glamor | 同"去掉 GBM"结果 |

**结论**：Deepin 202504 DDX 可在 Debian Trixie 上工作，但 vendor 提供的 GBM backend (`innogpu_gbm.so` → `libinnogpu_gbm.so`) 与 Debian 的 libgbm 不兼容。去掉它可以获得正常的 Xorg 显示，GL 由 Mesa swrast 提供。

## 当前库版本对比

| 文件 | 麒麟 V10 | Deepin 202504 |
|------|----------|---------------|
| innogpu_drv.so (DDX) | ABI 24 | ABI 25 ✓ |
| innogpu_dri.so (Mesa) | Mesa 22.1.3 | Mesa 23.1.3 |
| libEGL_inno.so | 有 | 有 |
| libGLX_inno.so | 有 | 有 |
| swrast_inno_dri.so | 无 | 有 |
| innogpu_gbm.so | 无 | 有（崩溃问题） |
| innogpu_drv_video.so | 无 | 有 |
| DKMS 源码 | 旧版 | 新版（~50文件差异） |

## 仓库文件

```
patches/001-kernel-6.12-compat.patch    — kernel 6.12 兼容补丁
scripts/install.sh                      — 从官方包安装的旧流程
scripts/build-patched6.sh               — 打包 patched-6（含 PLL workaround）
scripts/build-patched7.sh               — 打包 patched-7（含实验性用户态安装器）
scripts/patch-skip-first-gpupll.sh     — PLL workaround
scripts/disable-incompatible-userspace.sh — 恢复安全 modesetting
scripts/install-kylin-userspace.sh      — 从麒麟/UOS/Deepin root 安装用户态
scripts/test-xorg-once.sh              — 非重启 Xorg 测试
*.deb                                   — Release 包
```

## 快速安装（安全路径）

```bash
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-8.deb
sudo modprobe innogpu
cat /proc/driver/innogpu/gpu00/status   # 应显示 Driver OK, Firmware OK
```

## 下一步计划

1. [ ] 更新 install-kylin-userspace.sh：跳过 GBM backend
2. [ ] 创建 build-patched9.sh
3. [ ] 非重启验证：安装 Deepin 用户态（不含 GBM backend），测试 DDX + DRI
4. [ ] 如通过，启用开机自动加载，重启验证
5. [ ] 更新 README
