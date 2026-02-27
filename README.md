# innogpu-fh2m-debian-trixie

**Patches to compile the Innosilicon Fantasy II-M (innogpu fh2m) GPU kernel driver on Debian Trixie (kernel 6.12)**

[中文版](#中文说明) | [English](#english)

---

## 🚀 快速安装 / Quick Install

从 [Releases](https://github.com/timhant/innogpu-fh2m-debian-trixie/releases) 下载 `.deb` 包，三条命令搞定：

```bash
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-1.deb
sudo reboot
```

安装过程全自动：编译内核模块 → 安装固件 → 配置 X11 → 完成！

> 💡 如果你只是想让显卡工作，下载 .deb 安装即可，无需阅读以下技术细节。

---

## 中文说明

### 背景

[芯动科技（Innosilicon）](https://www.innosilicon.com/) 的 Fantasy II-M（风华2号-M）是一款国产 GPU，代号 fh2m。官方驱动仅支持 UOS、Kylin 等国产 Linux 发行版（均基于较旧的内核）。

本仓库提供了一组补丁，使 innogpu fh2m 驱动 **v3.3.3.42** 能够在 **Debian Trixie (13)** 的 **kernel 6.12** 上成功编译和运行。

### 硬件信息

| 项目 | 信息 |
|------|------|
| GPU | Innosilicon Fantasy II-M (风华2号-M) |
| PCI ID | `1ec8:9810` |
| 测试平台 | Suma-N40 笔记本 (Hygon C86 3350M, 16GB RAM) |
| 操作系统 | Debian Trixie (13), kernel 6.12.63+deb13-amd64 |
| 驱动版本 | innogpu-fh2m 3.3.3.42 |

### 补丁内容

本补丁修复了 **21 个源文件** 中的以下内核 API 不兼容问题：

| # | 问题 | 修复 | 涉及文件 |
|---|------|------|----------|
| 1 | `ioremap_nocache` 在 5.6+ 移除 | 替换为 `ioremap` | `inno_mm.c` |
| 2 | `PCI_IRQ_LEGACY` 在 6.8+ 重命名 | 替换为 `PCI_IRQ_INTX` | `inno_pci.c` |
| 3 | `thermal->devdata` 在 6.x 变为不透明 | 使用 `thermal_zone_device_priv()` | `hal_power.c` |
| 4 | `thermal->emul_temperature` 不可直接访问 | 移除 emul_temperature 逻辑 | `hal_power.c` |
| 5 | `thermal_zone_device_register_with_trips` 签名变更 | 移除 mask 参数 (6.10+) | `hal_power.c` |
| 6 | `drm_do_get_edid` 在 6.9+ 移除 | 用 `drm_edid_read_custom` 重新实现 | `compat_kernel6.h`, `inno_drm.c` |
| 7 | `drm_edid_block_valid` 在 6.9+ 移除 | 实现兼容函数 | `compat_kernel6.h` |
| 8 | `drm_driver.lastclose` 在 6.12 移除 | `#if` 版本守卫 | `pvr_drm.c` |
| 9 | `__assign_str` 宏参数变更 (2→1) | 条件编译适配 6.10+ | `pvr_fence_trace.h`, `rogue_trace_events.h` |
| 10 | `const static` 语序不符合标准 | 修正为 `static const` | 多个文件 |
| 11 | `platform_driver.remove` 返回类型变更 | 添加类型转换 | 多个文件 |
| 12 | **`FOP_UNSIGNED_OFFSET`** — kernel 6.12 新增要求 | 在 `file_operations` 中设置此 flag | `innogpu_drm.c` |
| 13 | Kbuild 编译警告过严 | 添加 `-Wno-error` flags | `Kbuild` |

### 安装步骤

#### 前提条件

1. Debian Trixie (13) 且内核版本 >= 6.9（主要针对 6.12 测试）
2. 从芯动科技获取官方驱动包：`innogpu-fh2m_3.3.3.42-driver-linux-desktop-sp-generic_amd64.deb`
3. 安装编译工具链：
   ```bash
   sudo apt install dkms build-essential linux-headers-$(uname -r)
   ```

#### 自动安装

```bash
git clone https://github.com/timhant/innogpu-fh2m-debian-trixie.git
cd innogpu-fh2m-debian-trixie
sudo ./scripts/install.sh /path/to/innogpu-fh2m_3.3.3.42-*.deb
sudo reboot
```

#### 手动安装

```bash
# 1. 安装官方驱动包（提取源码和固件）
sudo dpkg -i innogpu-fh2m_3.3.3.42-*.deb

# 2. 应用补丁
cd /usr/src/innogpu-kernel-2.2
sudo patch -p3 -N < /path/to/patches/001-kernel-6.12-compat.patch

# 3. 编译
sudo dkms build innogpu-kernel/2.2 -k $(uname -r) --force

# 4. 安装
sudo dkms install innogpu-kernel/2.2 -k $(uname -r) --force

# 5. 配置（参见 scripts/install.sh 中的 post-install 步骤）

# 6. 重启
sudo reboot
```

#### 验证

```bash
# 检查驱动加载
dmesg | grep innogpu
# 应看到: [drm] Initialized innogpu 2.19.x for 0000:01:00.0 on minor 0

# 检查设备节点
ls -la /dev/dri/card0

# 检查 GPU 信息
lspci -v -s $(lspci | grep 1ec8 | cut -d' ' -f1)
```

### 已知限制

1. **3D 加速**：用户空间库（libEGL、libGLX 等）与 Debian Trixie 的 mesa 版本不兼容（`_glapi_tls_Dispatch` 符号缺失）。当前使用 modesetting + 软件渲染，桌面流畅但无 GPU 加速。
2. **Wayland**：尚不支持，需使用 X11（GDM 中禁用 Wayland）。
3. **固件版本**：必须使用 v3.3 包中的固件（`fh2m.fw`），v3.2 固件与 v3.3 内核驱动不兼容。

### 故障排除：重启后黑屏 / GDM 无法启动

如果安装驱动后重启黑屏，GDM 日志显示 `Session never registered, failing`，通常是以下两个原因：

**问题 1：innogpu 用户空间库劫持系统 libgbm**

官方 deb 包会创建 `/etc/ld.so.conf.d/0-innogpu.conf`，将 innogpu 的用户空间库路径（`/usr/lib/x86_64-linux-gnu/innogpu-fh2m/`）加入动态链接搜索路径，且 `0-` 前缀使其优先级最高。

这会导致 Xorg 通过 modesetting → glamor → GBM 链路加载到 innogpu 的 `libgbm.so`，而该 libgbm 依赖 `innogpu_dri.so`。由于 innogpu_dri.so 与 mesa 不兼容（已被我们移除），libgbm 在 `gbm_create_device()` 时触发 **段错误（SEGV）**，Xorg 崩溃。

**修复：**
```bash
# SSH 登录后执行
sudo mv /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/0-innogpu.conf.disabled
sudo ldconfig
sudo systemctl restart gdm
```

> ⚠️ 系统更新或重新安装 innogpu deb 包后，`0-innogpu.conf` 可能会被重新创建，需再次执行上述命令。`install.sh` 脚本已自动处理此问题。

**问题 2：innogpu_dri.so 与 mesa 不兼容**

如果 `/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so` 存在，mesa 加载器会尝试加载它，导致 `undefined symbol: _glapi_tls_Dispatch` 错误。

**修复：**
```bash
sudo mv /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
        /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak
sudo systemctl restart gdm
```

### 故事

这套补丁诞生于 2026 年 2 月 8 日凌晨的一次通宵 debug session。一台搭载国产 Hygon CPU + 芯动 Fantasy II-M GPU 的笔记本（Suma-N40），安装 Debian Trixie 后 GPU 驱动完全无法编译。

经过数小时的内核源码分析和逐个修复 13 类 API 不兼容问题，最终在凌晨 2 点成功点亮了屏幕，让 GNOME 桌面在这台国产硬件上流畅运行。

---

## English

### Overview

This repository provides patches to compile the **Innosilicon Fantasy II-M (innogpu fh2m)** GPU kernel driver (v3.3.3.42) on **Debian Trixie (13)** with **kernel 6.12**.

The official driver only supports domestic Chinese Linux distributions (UOS, Kylin) based on older kernels. These patches fix 13 categories of kernel API incompatibilities introduced between kernel 5.x and 6.12.

### Quick Start

Download the `.deb` package from [Releases](https://github.com/timhant/innogpu-fh2m-debian-trixie/releases):

```bash
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-1.deb
sudo reboot
```

The package auto-compiles the kernel module via DKMS, installs firmware, and configures X11.

### Key Patches

- `ioremap_nocache` → `ioremap`
- `PCI_IRQ_LEGACY` → `PCI_IRQ_INTX`
- Thermal subsystem opaque struct migration
- `drm_do_get_edid` → `drm_edid_read_custom` (full reimplementation)
- `FOP_UNSIGNED_OFFSET` flag for kernel 6.12 DRM file operations
- `__assign_str` macro parameter change (6.10+)
- `drm_driver.lastclose` removal (6.12)
- Various `const` ordering and return type fixes

### Known Limitations

- **No 3D acceleration**: Userspace libraries incompatible with Debian Trixie's mesa
- **X11 only**: Wayland not yet supported
- **Firmware**: Must use v3.3 firmware (included in official package)

### Troubleshooting: Black Screen / GDM Fails After Reboot

If you see a black screen after installing the driver and GDM logs show `Session never registered, failing`, SSH into the machine and run:

```bash
# Fix 1: Remove innogpu ld.so override (hijacks system libgbm → Xorg SEGV)
sudo mv /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/0-innogpu.conf.disabled
sudo ldconfig

# Fix 2: Disable incompatible DRI driver (causes _glapi_tls_Dispatch error)
sudo mv /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
        /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak

# Restart display manager
sudo systemctl restart gdm
```

**Root cause**: The official deb creates `/etc/ld.so.conf.d/0-innogpu.conf`, which makes innogpu's `libgbm.so` take priority over mesa's. This innogpu libgbm tries to load `innogpu_dri.so`, which is incompatible with mesa, causing a segfault in `gbm_create_device()`. The `install.sh` script handles this automatically, but system updates may restore the file.

> ⚠️ After reinstalling the innogpu deb or running system updates, you may need to re-run these commands.

### Hardware Tested

- **GPU**: Innosilicon Fantasy II-M (PCI ID `1ec8:9810`)
- **CPU**: Hygon C86 3350M (x86_64, Zen-based)
- **Platform**: Suma-N40 laptop
- **OS**: Debian Trixie (13), kernel 6.12.63

### License

The patches themselves are released under **MIT License**.

The original driver source is © Innosilicon Technology Ltd., licensed under **Dual MIT/GPL**. You must obtain the official driver package from Innosilicon to use these patches.

---

*Made with 🔧 and ☕ at 2 AM, Feb 8, 2026*
