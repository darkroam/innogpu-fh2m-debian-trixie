# innogpu-fh2m-debian-trixie

Debian Trixie kernel 6.12 上的 Innosilicon Fantasy II-M / 风华 2 号 M（innogpu fh2m）驱动打包与兼容修复。

目标：安装后能为当前内核构建 DKMS 模块，手动验证加载成功后，再启用开机自动加载。

## 支持状态

已在以下内核上排障/验证过构建流程：

- 6.12.88+deb13-amd64
- 6.12.90+deb13-amd64

驱动来源：官方 innogpu-fh2m 3.3.3.42。

## 快速安装

从 Releases 下载最新 deb 后：

```bash
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo dpkg -i innogpu-fh2m-trixie_3.3.3.42-patched-5.deb
```

安装包会做这些事：

- 安装 DKMS 源码和固件
- 为当前内核构建并安装 innogpu 模块
- 写入 `options innogpu firmware_en=1`
- 禁用容易冲突的官方 userspace/GL 配置
- 安装 `innogpu-skip-first-gpupll` 辅助脚本
- 默认不启用开机自动加载，避免未验证模块导致重启黑屏或 Oops

## 验证模块已生成

Debian DKMS 可能把模块压缩安装为 `.ko.xz`，所以查询时用 `innogpu.ko*`：

```bash
/usr/sbin/dkms status | grep innogpu
find /lib/modules/$(uname -r) -iname 'innogpu.ko*'
```

正常应看到类似：

```text
innogpu-kernel/2.2, 6.12.90+deb13-amd64, x86_64: installed
/lib/modules/6.12.90+deb13-amd64/updates/dkms/innogpu.ko.xz
```

## 6.12.88 / 6.12.90 主要思路

1. 先用 DKMS 为目标内核构建模块。
2. 确认 `/etc/modprobe.d/innogpu.conf` 包含：
   ```text
   options innogpu firmware_en=1
   ```
3. 如果模块在 `set_pll_reg -> g0m_soc_setpll -> g0m_soc_hw_init` 附近 Oops，使用二进制 workaround：跳过第一次 GPU PLL 初始化调用。
4. 只在手动 `modprobe innogpu` 成功、没有 Oops/soft lockup 后，再启用开机加载。
5. 更新 initramfs 时只用发行版工具 `update-initramfs`，不要手动 cpio 解包/重打 initrd。

## 应用 PLL workaround

如果是 6.12.88/6.12.90，建议在首次手动加载前先执行：

```bash
sudo innogpu-skip-first-gpupll $(uname -r)
```

脚本会：

- 自动处理 DKMS 默认路径 `/lib/modules/$KVER/updates/dkms/innogpu.ko.xz`
- 解压/复制到 `/lib/modules/$KVER/kernel/drivers/gpu/drm/innogpu/innogpu.ko`
- 备份原模块
- 将第一次 `g0m_soc_hw_init -> g0m_soc_setpll` 调用改成 NOP
- 运行 `depmod -a $KVER`

如果普通用户 PATH 找不到命令，可用完整路径：

```bash
sudo /usr/bin/innogpu-skip-first-gpupll $(uname -r)
```

## 手动加载验证

```bash
sudo modprobe innogpu

find /lib/modules/$(uname -r) -iname 'innogpu.ko*'
ls -l /dev/dri /dev/fb* 2>/dev/null || true
cat /sys/module/innogpu/parameters/firmware_en
lspci -nnk | grep -EA4 '1ec8|VGA|Display|3D'
```

成功后通常应看到：

- `/dev/dri/card0`
- `/dev/dri/renderD128`
- `/sys/module/innogpu/parameters/firmware_en = 1`
- `lspci` 中 `Kernel driver in use: inno-drv`

如果 `modprobe` 出现 Oops/soft lockup：

```bash
sudo rm -f /etc/modules-load.d/innogpu.conf
sudo depmod -a $(uname -r)
sudo reboot
```

重启后再从干净状态排查，不要在 Oops 后反复 rmmod/modprobe。

## 启用开机自动加载

确认手动加载稳定后再启用：

```bash
printf '%s\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf
sudo depmod -a $(uname -r)
sudo update-initramfs -u -k $(uname -r)
sudo reboot
```

重启后验证：

```bash
lsmod | grep '^innogpu'
ls -l /dev/dri /dev/fb* 2>/dev/null || true
cat /sys/module/innogpu/parameters/firmware_en
```

## 如果安装后找不到模块或命令

模块查询：

```bash
find /lib/modules/$(uname -r) -iname 'innogpu.ko*'
/usr/sbin/dkms status | grep innogpu
```

注意 `.ko.xz` 是正常的 DKMS 压缩模块，不是缺失。

命令查询：

```bash
command -v innogpu-skip-first-gpupll
ls -l /usr/bin/innogpu-skip-first-gpupll /usr/sbin/innogpu-skip-first-gpupll
```

## 本仓库补丁包含

- kernel 6.9+ / 6.12 API 兼容修复
- `flush_workqueue(system_wq)` 构建警告修复
- `firmware_en=1` 默认配置
- 禁用有冲突的官方 GL/userspace 配置
- `innogpu-skip-first-gpupll` runtime workaround
- DKMS/postinst PATH 修复，避免找不到 `/usr/sbin/dkms`
- 兼容 DKMS 生成的 `updates/dkms/innogpu.ko.xz`

## 文件

- `patches/001-kernel-6.12-compat.patch`：源码兼容补丁
- `scripts/install.sh`：从官方包安装并打补丁的旧流程
- `scripts/patch-skip-first-gpupll.sh`：PLL workaround 脚本
- `innogpu-fh2m-trixie_3.3.3.42-patched-*.deb`：Release 包，不纳入 git 跟踪

## 安全原则

- 不要在模块尚未手动验证成功前启用 `/etc/modules-load.d/innogpu.conf`
- 不要手动 cpio 重打 initrd
- Oops 后先禁用 autoload 并重启到干净状态，再继续测试
