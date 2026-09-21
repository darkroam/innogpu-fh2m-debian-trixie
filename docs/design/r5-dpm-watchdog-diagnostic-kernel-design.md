# R5 DPM watchdog 诊断内核设计

- 状态：**待 qoder 初审、dsh 终审；仅设计，不构建、不安装、不执行**
- 日期：2026-09-18
- 基线：Debian `linux 6.12.101-1`、运行内核 `6.12.101+deb13-amd64`、驱动 `5.0.0-i6`
- 目标：用内核原生 DPM watchdog 直接报告超时的设备 PM callback、被卡任务栈和设备名
- 冻结：`R5=FAIL`、`U1/U2`、`validation-results`、签发和 tag 均保持冻结

## 1. 决策

不继续 SysRq round-2，也不自行给 PM core 写新的超时器。精确的 Debian 6.12.101 源码已经提供 `CONFIG_DPM_WATCHDOG`：它在设备 suspend/resume callback 周围启动 timer；超时后执行：

```text
dev_emerg(dev, "**** DPM device timeout ****")
show_stack(callback_task)
panic("<driver> <device>: unrecoverable failure")
```

这比物理键盘 SysRq 更确定，也避开了 PM 窗口中 `hung_detector_suspended=true` 导致 hung-task 检测器不可用的问题。首轮诊断内核只改变配置，不改一行内核 C 源码。

## 2. 已复核事实

### 2.1 精确源码

APT 仍提供与运行内核同源的包：

```text
package=linux-source-6.12
version=6.12.101-1
file=linux-source-6.12_6.12.101-1_all.deb
sha256=9a5ea91bb8dc78a2a43f190de3c60f44cad91d6164556d0a3676aa1024342303
source_makefile=VERSION 6 / PATCHLEVEL 12 / SUBLEVEL 101
```

该 deb 已在 `/tmp` 只读下载并复算 SHA 一致；未安装。正式下载命令必须显式锁定版本：

```bash
apt-get download linux-source-6.12=6.12.101-1
```

禁止使用未带版本的 `apt-get download linux-source-6.12`，因为当前 APT candidate 已是 `6.12.107-1`。正式构建必须重新从受控输入目录展开并复算，不得复用 `/tmp` 调查树。

### 2.2 原生 watchdog 覆盖

`drivers/base/power/main.c` 的 `dpm_watchdog_set()` 使用 `CONFIG_DPM_WATCHDOG_TIMEOUT` 启动 timer，handler 输出设备、`show_stack()` 后 panic。当前基线配置已有：

```text
CONFIG_PM_DEBUG=y
CONFIG_PM_SLEEP_DEBUG=y
CONFIG_EXPERT=y
CONFIG_PSTORE=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
CONFIG_UNWINDER_ORC=y
CONFIG_DEBUG_INFO_BTF=y
# CONFIG_DPM_WATCHDOG is not set
```

该版本只有两个 watchdog 设置点：普通 `device_suspend()` 和 `device_resume()`。本案执行 `pm_test=devices`，PM core 在 `dpm_suspend_start()` 完成普通设备 suspend 后即转入恢复，不进入 late/noirq，所以这两个设置点覆盖本轮目标 callback。普通 suspend 路径中 timer 在 `device_lock()` 和 callback 前启动，因此能捕获设备锁等待和 callback 内等待；异步 PM worker也各自拥有 watchdog 与任务指针。

明确不覆盖：

- `dpm_prepare()` callback；
- `device_suspend()` 内 watchdog 启动前的 `dpm_wait_for_subordinate()`、`pm_runtime_barrier()` 和相关 `dpm_list_mtx` 等待；
- `device_resume()` 内 watchdog 启动前的 `dpm_wait_for_superior()`，以及 watchdog 清除后的 `dpm_complete()` callback；
- late/noirq callback，以及 PM core 中其他不属于已包围设备处理区间的等待；
- timer 中断本身无法运行的全 CPU/中断失效情形。

因此“watchdog 未 panic”只表示挂点在覆盖外或 timer 不可达，不能表示 R5 已恢复。

### 2.3 round-1 约束

watchdog round-1 已记录：soft/hard lockup 检测器 armed 后，`pm_test=devices` 挂死至少 207 秒仍未 panic，EFI pstore 为空。该结果只排除已配置检测器可达的自旋类现场，挂死仍为 `unclassified_leaning_dstate`。

本机 `systemd-pstore.service` 已启用并在 `sysinit.target` 前归档 `/sys/fs/pstore` 到 `/var/lib/systemd/pstore`。round-1 的自建 collector 缺 `[Install]` 是独立缺陷；本方案不再复制该服务，直接使用已验证的系统原生归档并通过前后清单识别新增记录。

Linux 没有 `/sys/class/pstore/backend`；round-1 收集器由此得到的 `pstore_backend=unknown` 不能作为健康门。诊断内核使用本机已验证的真实接口：

```text
/sys/module/efi_pstore exists
/sys/module/efi_pstore/parameters/pstore_disable=N
/sys/module/efi_pstore/parameters/record_size=1024
/sys/fs/pstore mounted with fstype=pstore
```

任一条件不成立即停止，不执行 `pm_test`。

## 3. 诊断内核配置

正式构建从 `/boot/config-6.12.101+deb13-amd64` 复制 `.config`，只允许以下三项语义差异：

```diff
-CONFIG_LOCALVERSION=""
+CONFIG_LOCALVERSION="-r5dpm1"
-# CONFIG_DPM_WATCHDOG is not set
+CONFIG_DPM_WATCHDOG=y
+CONFIG_DPM_WATCHDOG_TIMEOUT=20
```

生成的内核 release 固定为：

```text
6.12.101-r5dpm1
```

选择 20 秒的理由：解绑态 `pm_test=devices` 实测约 6 秒，其中包含 5 秒 `pm_test_delay`，而 watchdog 是按单设备区间计时；20 秒足以避开正常测试延迟并显著短于此前 120 秒以上的硬挂观察。超时值不是运行时旋钮，评审后不得临场修改。

保持 `CONFIG_PSTORE_CONSOLE` 关闭。DPM watchdog 自身会 panic，pstore 的 kmsg dumper 会保存 panic 尾部；开启 console frontend 只会持续占用 EFI 的有限记录空间，不增加本轮必要信息。

配置门禁：

1. `make olddefconfig` 后用 `scripts/diffconfig` 对比基线；除上述三项外出现任何差异即停止。
2. `make -s kernelrelease` 必须精确输出 `6.12.101-r5dpm1`。
3. 构建后的 `/boot/config-6.12.101-r5dpm1` 必须再次断言 watchdog、20 秒、pstore、符号和 ORC 配置。
4. 不改变 `pm_async`，避免通过串行化掩盖原有排序问题。

## 4. 构建与产物

### 4.1 构建环境

构建前只安装 Debian 原生构建依赖；当前预检缺少 `flex`、`bison`、`rsync`、`libelf-dev`，必须补齐。其余依赖清单由 `make olddefconfig`/`bindeb-pkg` 的实际缺项生成，不增加无关工具。

源码、输出和 deb 全部放在 Git 忽略目录：

```text
.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/
```

禁止把内核源码、对象或 deb 加入 Git；Git 只保留脱敏的输入/配置/产物 SHA 摘要。

### 4.2 可复现构建

使用独立 `O=build-A` 和 `O=build-B`，固定：

```text
ARCH=x86_64
KBUILD_BUILD_USER=codex-r5
KBUILD_BUILD_HOST=diagnostic
KBUILD_BUILD_TIMESTAMP=2026-09-18T00:00:00Z
SOURCE_DATE_EPOCH=1789689600
KDEB_PKGVERSION=6.12.101-r5dpm1-1
```

以 `make -j$(nproc) bindeb-pkg` 生成 image 和 headers deb。A/B 至少对以下内容做字节一致校验：

- 内核 image deb；
- headers deb；
- 解包后的 `vmlinuz`、`System.map`、`.config` 和 `Module.symvers`。

若 deb 因打包元数据不能字节一致，必须先定位时间戳/文件顺序来源；不得用“解包内容相同”掩盖未解释的包差异。

### 4.3 静态验收

安装前记录：

```text
source_deb_sha256
baseline_config_sha256
diagnostic_config_sha256
config_diff
kernelrelease
image_deb_sha256_A/B
headers_deb_sha256_A/B
vmlinuz_sha256_A/B
System.map_sha256_A/B
Module.symvers_sha256_A/B
```

并对构建产物反汇编/符号核验 `dpm_watchdog_handler` 存在，字符串中包含 `DPM device timeout` 和 `unrecoverable failure`。这只验证机制进入二进制，不代替真机超时验证。

## 5. 厂商模块兼容

诊断内核有独立 release，不能直接复用 `6.12.101+deb13-amd64` 的 `fantgpu.ko`。安装 image/headers 后、重启前，必须针对 `6.12.101-r5dpm1` 单独构建并安装当前 i6 源树的 DKMS 模块：

```text
fantgpu-fh2m-kernel/2.2
```

安装前门禁：

1. `/usr/src/fantgpu-fh2m-kernel-2.2` 仍对应 i6 运行树和既有 hash；
2. DKMS 只针对新 release 构建，不删除原内核实例；
3. `modinfo` 的 vermagic 必须以 `6.12.101-r5dpm1` 开头；
4. `depmod` 和 initramfs 中必须能找到新模块；
5. 新内核 image、headers、initramfs 和 GRUB 菜单项存在；原 `6.12.101+deb13-amd64` image、modules 和菜单项仍存在。

任何 DKMS、vermagic、initramfs 或 GRUB 检查失败都不得重启到诊断内核。已知的 `postinst_current_kernel_only` 发布缺陷不得用来猜测安装成功；本诊断线显式指定目标 kernel release。

## 6. 操作位置

每个执行脚本都必须提前输出下一步所在环境：

| 阶段 | 是否可在 dwm | 执行位置 |
|---|---|---|
| 下载、校验、解包、双构建、静态检查 | 可以 | 普通终端；不需要退出桌面 |
| 安装新 image/headers、目标 DKMS、initramfs、GRUB 检查 | 可以，但不得卸载/重载当前 GPU 模块 | 普通终端，用户在场 |
| 重启并选择诊断内核 | 不可以 | 本地物理控制台/GRUB |
| 诊断内核健康前置 | 不可以 | 本地 TTY；不得 `startx` |
| 唯一一次 `pm_test=devices` | 不可以 | 本地 TTY；触发器用独立 root systemd service |
| panic 自动重启后的证据收集 | 不可以 | 本地 TTY；先收集再启动桌面 |
| 收集和恢复全部 PASS 后 | 可以 | 本地 TTY 执行 `startx` |

SSH 不作为正式触发、存活或恢复判据。显示器可以是内屏或外接屏；关键是本地 TTY，不能是 `dwm`/X 会话。

## 7. 安装、启动与回退

### 7.1 首次启动

安装不改变原内核文件。当前本机 `GRUB_DEFAULT=0` 且装有 `6.12.107`，所以“安装后默认会回到原 6.12.101”不成立。安装脚本必须先备份 GRUB 配置和环境，再建立以下受控启动关系：

1. 从新生成的 `grub.cfg` 解析原 `6.12.101+deb13-amd64` 与诊断 `6.12.101-r5dpm1` 的精确 menuentry ID；名称模糊、缺失或重复即停止。
2. 临时配置 `GRUB_DEFAULT=saved`，运行 `update-grub`。
3. 用 `grub-set-default <原 6.12.101 ID>` 设置持久安全项，并从 `grub-editenv list` 读回精确匹配。
4. 用 `grub-reboot <诊断内核 ID>` 只安排下一次进入诊断内核，并读回 `next_entry` 精确匹配。
5. 完成取证后恢复原 GRUB 配置；在此之前不得把诊断内核设为 saved default。

临时 GRUB 配置全局追加：

```text
panic=10 no_console_suspend ignore_loglevel
```

进入后先在本地 TTY 验证：

```text
uname -r=6.12.101-r5dpm1
/proc/cmdline contains panic=10 no_console_suspend ignore_loglevel
CONFIG_DPM_WATCHDOG=y
CONFIG_DPM_WATCHDOG_TIMEOUT=20
/sys/module/efi_pstore exists
efi_pstore pstore_disable=N
efi_pstore record_size=1024
/sys/fs/pstore fstype=pstore
fantgpu vermagic matches running kernel
Driver/Firmware=OK
PCI bound to fant-drv
card0/renderD128/fb0 present
pm_test=[none]
pm_debug_messages=0
```

再运行非挂起健康检查；任何 Oops、模块错误或图形节点缺失都直接回原内核，不执行 `pm_test`。

### 7.2 回退

诊断内核启动失败时，下一次启动应由 saved default 自动进入原 `6.12.101+deb13-amd64`；若未自动进入，则在 GRUB 手工选择该项。该回退不需要卸载 i6，也不覆盖原模块。回到原内核后先确认：

```text
uname -r=6.12.101+deb13-amd64
Driver/Firmware=OK
pm_test=[none]
pm_debug_messages=0
```

诊断完成前不删除诊断内核；证据闭合后再由 dsh 决定是否卸载。若原内核也无法启动，沿用已批准的 `4.0.2-i3` SHA 回退卡，不在本设计中发明第二套恢复链。

## 8. 唯一一次诊断运行

执行前先退出 `dwm/startx` 并停留在本地 TTY。脚本保存当前 boot ID、pstore 与 `/var/lib/systemd/pstore` 清单、驱动健康、节点和 PM 状态，写入 `armed` 后 `sync`。随后必须通过 `systemd-run` 创建独立于登录会话的 root 瞬态单元，使用唯一固定 unit 名、`Type=oneshot` 和 `TimeoutStartSec=infinity`，由该单元单次执行：

```text
echo devices > /sys/power/pm_test
echo mem > /sys/power/state
```

禁止真实 S3、第二次 devices、stop-stage 和 SysRq round-2。

预期路径：

1. 若设备 callback 卡住超过 20 秒，DPM watchdog 输出设备名、该任务栈并 panic；
2. `panic=10` 约 10 秒后自动重启；
3. GRUB 默认仍指向原安全内核，自动回到原 `6.12.101+deb13-amd64`；
4. 用户留在本地 TTY，不进入 `dwm`，执行受审收集器；
5. 收集器比较前后清单，从 `/var/lib/systemd/pstore` 和仍存在的 `/sys/fs/pstore` 复制新增记录，提取 timeout 设备、panic 行、RIP 和 stack。

若 60 秒未自动重启，只允许长按电源键冷启动一次，随后按同一收集流程处理；不得重复实验。

## 9. 判定

运行时收集器只保存原始证据并输出 `r5_dpm_watchdog_capture=UNCLASSIFIED`；不得在恢复现场用脆弱的文本启发式自动升级结论。qoder/dsh 复核 pstore、测试 boot journal、触发返回证据和恢复模式后，按下表落正式四分支判定。`systemd-run` 非零且执行边界不明时记 `FAILED_OR_UNVERIFIED`，不得当作 `NOT_REPRODUCED`。

| 结果 | 判定 | 归因上限 |
|---|---|---|
| pstore 含 `DPM device timeout`、设备名和可读 stack | `DPM_WATCHDOG_CAPTURE=LOCATED` | 该设备处理区间为高可信候选；最高 vendor frame 为候选挂点 |
| 有 timeout/panic 但 stack 截断 | `DPM_WATCHDOG_CAPTURE=PARTIAL` | 设备已定位，函数未定位；不重复本轮 |
| 无 panic，`pm_test` 正常返回 | `DPM_WATCHDOG_CAPTURE=NOT_REPRODUCED` | 只说明本轮未复现，R5 仍为 FAIL |
| 仍硬挂且无 DPM panic | `DPM_WATCHDOG_CAPTURE=OUTSIDE_COVERAGE` | 挂点位于 watchdog 启动前/PM core 等待，或 timer 不可达 |
| 诊断内核健康门失败 | `DPM_WATCHDOG_CAPTURE=NOT_RUN` | 构建/兼容问题，禁止挂起测试 |

若输出设备为 FT/PVR/DRM 子设备，进入对应 callback 的源码或闭源边界分析；若设备为父 PCI 或其他设备，按实际设备处理，不预设 fantgpu。即使捕获 stack，`R5=FAIL` 也不会自动转 PASS。

## 10. 证据与门禁

原始构建、panic 和 pstore 文件只进入 ignored 归档。Git 只保留脱敏摘要：文件名、SHA-256、字节数、脱敏规则和归档相对路径。必须记录：

```text
r5_investigation_stage=dpm_watchdog_diagnostic_kernel
r5_diagnostic_kernel=6.12.101-r5dpm1
r5_dpm_watchdog_timeout=20
r5_dpm_timeout_device=<name-or-UNAVAILABLE>
r5_detector_reported_task=<comm/pid-or-UNAVAILABLE>
r5_pstore_stack_top=<symbol-or-UNAVAILABLE>
r5_highest_vendor_frame=<symbol-or-UNAVAILABLE>
r5_dpm_watchdog_capture=UNCLASSIFIED  # 运行时原始收集结果
r5_dpm_watchdog_capture_reviewed=LOCATED|PARTIAL|NOT_REPRODUCED|OUTSIDE_COVERAGE|NOT_RUN
r5_root_cause=<candidate-or-unresolved>
r5_validation_status=FAIL
```

设计提交前运行 `git diff --check`、`scripts/check-docs.sh`、`python3 tools/audit-licenses.py`、协作校验和 R16 gate；allowlist 只由 dsh 在 commit-time 正式重生成。

## 11. 放行流程

1. codex 提交本设计决定；
2. qoder 初审原生 watchdog 覆盖边界、配置 diff、DKMS/回退和 TTY 规则；
3. dsh 终审设计；
4. 批准后 codex 生成构建/验收脚本并完成双构建，不安装；
5. qoder 初审构建产物，dsh 终审后才安装；
6. 诊断内核首次启动健康门另行放行；
7. 唯一一次 `pm_test=devices` 再由 dsh 明示放行，用户在场监督。

设计、构建或启动通过都不等于 R5 通过，也不解除任何冻结项。

---

## 附录 · 2026-09-21 修订注（R18 文档迭代补记，dsh 裁定 E51）

本节为文档迭代期的**追加补记**，上文 §3 原文（「三项」及其行文）保持历史审查版本不动。**§3 读者应先读本注**（签署两行规范化的差异口径见下）。

- 差异事实：§3 的文字为「只允许三项语义差异（LOCALVERSION / DPM_WATCHDOG /
  DPM_WATCHDOG_TIMEOUT）」，而实际构建结果（r5-dpm-watchdog-kernel-build-result.txt）
  的 config_delta 另含签署两行规范化（MODULE_SIG_ALL=y + MODULE_SIG_KEY=certs/
  r5-signing-key.pem），当时经 dsh P2-1 裁定接受（olddefconfig 不可避免默认 + 固定 key
  为 A/B 字节一致前提；R5 语义影响零）。
- 处置口径：r5dpm2 设计已按「三项 + 签署两行规范化」表述（8a667f6fe30c，L237 起
  「已规范化基线」）；本文件因当时处于冻结期未改，现在非冻结周期以追加注补记，
  不改历史正文、不改历史哈希锚（115368769d53 仍为审查版本身份）。
