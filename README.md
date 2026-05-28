# innogpu-fh2m-debian-trixie

**Patches to compile the Innosilicon Fantasy II-M (innogpu fh2m) GPU kernel driver on Debian Trixie (kernel 6.12)**

---

## 🚀 快速安装 / Quick Install

从 [Releases](https://github.com/timhant/innogpu-fh2m-debian-trixie/releases) 下载 `.deb` 包，三条命令搞定：

```bash
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-2.deb
sudo reboot
```

安装过程全自动：编译内核模块 → 安装固件 → 配置 X11 → 完成！

> 💡 如果你只是想让显卡工作，下载 .deb 安装即可，无需阅读以下技术细节。
>
> ⚠️ Debian Trixie `6.12.88+deb13-amd64` 上的实测结果见下方“6.12.88+deb13-amd64 实测安装方案”。该内核还需要一个额外的 `flush_workqueue(system_wq)` 编译修复，以及在部分 Fantasy II-M 机器上跳过第一次 GPU PLL 初始化调用，否则可能在 `set_pll_reg` 处 Oops。

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
| 测试平台 | X7h笔记本 |
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


### 6.12.88+deb13-amd64 实测安装方案

本节记录在 Debian Trixie `6.12.88+deb13-amd64` 上实际点亮 Fantasy II-M 的完整思路，便于重装系统后按本仓库复现。

#### 核心思路

1. 先让官方 `innogpu-fh2m_3.3.3.42` 包释放 DKMS 源码和固件。
2. 对 `/usr/src/innogpu-kernel-2.2` 应用 kernel 6.12 兼容补丁。
3. 额外处理 kernel 6.12.88 的 `flush_workqueue(system_wq)` 警告：6.12 会对 flush system-wide workqueue 报 warning，驱动构建时又启用了 `-Werror`，因此必须注释掉 `innogpu/inno_task.c` 中的该调用。本仓库的 `patches/001-kernel-6.12-compat.patch` 已包含这个修复。
4. 如果模块能编译但 `modprobe innogpu` 在 `set_pll_reg -> g0m_soc_setpll -> g0m_soc_hw_init` Oops，说明该机器的第一次 GPU PLL 初始化会访问无效寄存器地址。当前可用 workaround 是对最终 `innogpu.ko` 做二进制补丁，把 `g0m_soc_hw_init` 中第一次调用 `g0m_soc_setpll` 的 `call` 改成 5 个 NOP。
5. 只在手动加载成功后再启用开机加载。不要在会 Oops 的模块上直接配置 `/etc/modules-load.d/innogpu.conf`。
6. 持久化时只使用发行版工具更新 initramfs：`update-initramfs -u -k 6.12.88+deb13-amd64`。不要手动 cpio 解包/重打 initrd，容易破坏启动所需 metadata/hooks，导致 VFS/rootfs panic。

#### 重装后推荐步骤

```bash
# 0. 确认目标内核
uname -r
# 期望: 6.12.88+deb13-amd64

# 1. 安装依赖和官方驱动包
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo dpkg -i /path/to/innogpu-fh2m_3.3.3.42-*.deb || sudo apt -f install

# 2. 应用本仓库补丁
cd /usr/src/innogpu-kernel-2.2
sudo patch -p3 -d / -N --fuzz=3 < /path/to/innogpu-fh2m-debian-trixie/patches/001-kernel-6.12-compat.patch

# 3. DKMS 构建和安装
sudo dkms build innogpu-kernel/2.2 -k $(uname -r) --force
sudo dkms install innogpu-kernel/2.2 -k $(uname -r) --force

# 4. 基础配置：必须开启 firmware_en
printf '%s\n' 'options innogpu firmware_en=1' | sudo tee /etc/modprobe.d/innogpu.conf
sudo depmod -a $(uname -r)
```

#### 如果 `modprobe innogpu` 在 set_pll_reg Oops

仓库提供了可复用脚本：

```bash
sudo /path/to/innogpu-fh2m-debian-trixie/scripts/patch-skip-first-gpupll.sh $(uname -r)
```

脚本会备份当前 `innogpu.ko`，把第一次 GPU PLL 初始化调用改成 NOP，并自动运行 `depmod`。

如果需要手动处理，先不要配置开机加载。如果已经配置过，请先禁用：

```bash
sudo rm -f /etc/modules-load.d/innogpu.conf
sudo depmod -a $(uname -r)
sudo reboot
```

重启到干净状态后，对安装后的模块做 binary workaround：

```bash
KVER=$(uname -r)
KO=/lib/modules/$KVER/kernel/drivers/gpu/drm/innogpu/innogpu.ko
sudo mkdir -p /lib/modules/$KVER/kernel/drivers/gpu/drm/innogpu/backup
sudo cp -a "$KO" "/lib/modules/$KVER/kernel/drivers/gpu/drm/innogpu/backup/innogpu.ko.pre-skip-first-gpupll"

sudo python3 - <<'PY'
from pathlib import Path
import subprocess
kver = subprocess.check_output(['uname', '-r'], text=True).strip()
p = Path(f'/lib/modules/{kver}/kernel/drivers/gpu/drm/innogpu/innogpu.ko')
b = bytearray(p.read_bytes())
# For innogpu-fh2m 3.3.3.42 on 6.12.88, .text file offset is 0x40 and
# the first g0m_soc_hw_init -> g0m_soc_setpll call is at .text+0x46392.
off = 0x40 + 0x46392
old = bytes.fromhex('e8 09 fd ff ff')
new = bytes.fromhex('90 90 90 90 90')
if b[off:off+5] == new:
    print('already patched')
elif b[off:off+5] == old:
    b[off:off+5] = new
    p.write_bytes(b)
    print('patched skip-first-gpupll')
else:
    raise SystemExit(f'unexpected bytes at {off:#x}: {b[off:off+5].hex(" ")}')
PY

sudo depmod -a "$KVER"
```

如启用了 Secure Boot 或模块签名策略，需要在二进制补丁后重新签名模块；否则签名会失效。

#### 手动加载验证

```bash
sudo dmesg -C
sudo modprobe drm
sudo modprobe drm_display_helper
sudo modprobe drm_kms_helper
sudo modprobe i2c-algo-bit
sudo modprobe snd-pcm
sudo modprobe innogpu

lspci -nnk | grep -EA4 '1ec8|VGA|Display|3D'
ls -l /dev/dri /dev/fb0
cat /sys/module/innogpu/parameters/firmware_en
```

成功状态应类似：

```text
Kernel driver in use: inno-drv
/dev/dri/card0
/dev/dri/renderD128
/dev/fb0
/sys/module/innogpu/parameters/firmware_en = 1
/sys/class/graphics/fb0/name = innogpudrmfb
```

确认没有 Oops/soft lockup 后，再启用开机加载和 initramfs 更新：

```bash
printf '%s\n' 'innogpu' | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a $(uname -r)
sudo update-initramfs -u -k $(uname -r)
sudo reboot
```

重启后再次验证：

```bash
uname -r
lsmod | grep '^innogpu'
lspci -nnk | grep -EA4 '1ec8|VGA|Display|3D'
ls -l /dev/dri /dev/fb0
cat /sys/module/innogpu/parameters/firmware_en
```

#### Xorg / TTY 登录修复

本驱动目前建议使用 X11 modesetting，不要加载官方不兼容的 Mesa/GBM 用户态库：

```bash
# 禁用官方 ld.so override，避免 innogpu libgbm 劫持系统 Mesa
sudo mv /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/0-innogpu.conf.disabled 2>/dev/null || true
sudo ldconfig

# 防止官方服务开机重新创建 0-innogpu.conf
sudo rm -f /etc/systemd/system/sw-inno-gl.service /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service
sudo ln -sf /dev/null /etc/systemd/system/sw-inno-gl.service
sudo systemctl daemon-reload

# 禁用与 Debian Mesa 不兼容的 DRI driver
sudo mv /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
        /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak 2>/dev/null || true

# 使用 modesetting DDX
sudo tee /etc/X11/xorg.conf >/dev/null <<'XORG'
Section "Device"
    Identifier "gpu"
    Driver "modesetting"
    Option "kmsdev" "/dev/dri/card0"
EndSection
XORG
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

> ⚠️ 官方 innogpu-fh2m 包还附带了 `sw-inno-gl.service`（systemd 服务），每次开机都会重新创建 `0-innogpu.conf`。必须 mask 掉：
> ```bash
> sudo rm -f /etc/systemd/system/sw-inno-gl.service /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service
> sudo ln -sf /dev/null /etc/systemd/system/sw-inno-gl.service
> sudo systemctl daemon-reload
> ```
> `install.sh` 和 patched-2 的 .deb 包已自动处理此问题。

**问题 2：innogpu_dri.so 与 mesa 不兼容**

如果 `/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so` 存在，mesa 加载器会尝试加载它，导致 `undefined symbol: _glapi_tls_Dispatch` 错误。

**修复：**
```bash
sudo mv /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
        /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak
sudo systemctl restart gdm
```
