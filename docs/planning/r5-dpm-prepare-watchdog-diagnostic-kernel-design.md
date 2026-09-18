# R5 dpm_prepare/dpm_complete/noirq watchdog 诊断内核设计（6.12.101-r5dpm2）

- 状态：**待 qoder 初审、dsh 终审；本轮仅设计，不构建、不安装、不重启、不执行 `pm_test`**
- 日期：2026-09-18
- 源码基线：Debian `linux-source-6.12=6.12.101-1`
- 运行安全基线：原内核 `6.12.101+deb13-amd64`、驱动 `5.0.0-i6`
- 诊断内核目标：`6.12.101-r5dpm2`
- 冻结：`R5=FAIL`；既有实验禁止重跑；`U1/U2`、`validation-results`、签发和 tag 均保持冻结

## 1. 定位与决策

步骤 7 已由 qoder/dsh 复核并接受为 `OUTSIDE_COVERAGE`：本地 TTY 健康门通过，唯一一次 `pm_test=devices` 已确认进入 `PM: suspend entry (deep)`，但约 3 分钟内没有 DPM watchdog timeout、panic、自动重启或新增 pstore，最终冷启动恢复。该结果降低了现有 watchdog 已武装的普通 `device_suspend()`/`device_resume()` 回调区间作为挂点的可能性，但不能排除 timer 不可达；根因仍未定位。

本阶段只扩展 Linux PM core 已有的 `CONFIG_DPM_WATCHDOG`：

1. 在 `device_prepare()` 的每设备临界区武装 watchdog，覆盖 `device_lock()` 和 `->prepare()` 回调；
2. 在 `device_complete()` 的回调调用区间武装同一 watchdog；
3. 在 `device_suspend_noirq()` 的回调调用区间武装同一 watchdog；
4. 不新建 timer、线程、驱动接口或状态结构，不改 fantgpu 源码和任何共享 ABI；
5. 诊断产物使用新 release `6.12.101-r5dpm2`，不覆盖 `r5dpm1`；
6. 运行仍只允许一次 `pm_test=devices`。

有一个必须前置写明的边界：`pm_test=devices` 在调用 `suspend_enter()` 之前结束，因此不会执行 late/noirq。新增的 `device_prepare()`、`device_complete()` 位于该模式实际路径上，会在唯一运行中动态验证；`device_suspend_noirq()` hunk 按本设计构建，但本轮只做源码、编译和产物验证。若未来要动态验证 noirq，必须另立设计并由 dsh 单独放行更深的 `pm_test` 层级；本设计不授权该实验。

## 2. 6.12.101 源码复核

只读源码锚点：

```text
.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-kernel/source/linux-source-6.12/
```

### 2.1 现有 watchdog

`drivers/base/power/main.c`：

- `498-559`：`struct dpm_watchdog`、`DECLARE_DPM_WATCHDOG_ON_STACK`、handler、`dpm_watchdog_set()` 和 `dpm_watchdog_clear()`；
- `device_resume()`：基线 `946` 武装、`994` 清除；武装晚于 `dpm_wait_for_superior()`；
- `device_suspend()`：基线 `1658` 武装、`1710` 清除；武装晚于 `dpm_wait_for_subordinate()` 和 `pm_runtime_barrier()`，早于 `device_lock()` 与普通 suspend callback；
- 6.12.101 基线只有上述两个 `dpm_watchdog_set()` 调用点。

handler 通过 timer 输出：

```text
**** DPM device timeout ****
show_stack(wd->tsk)
<driver> <device>: unrecoverable failure
```

随后 panic；`panic=10` 负责自动重启，EFI pstore 负责保存 panic kmsg。timer 无法调度时，任何新增 hunk 都不能保证触发。

### 2.2 `pm_test=devices` 实际执行路径

`kernel/power/suspend.c` 的顺序为：

1. `pm_suspend()` `:624` 先打印 `PM: suspend entry (deep)`，随后 `:625` 调用 `enter_state()`；该打印只证明进入 `pm_suspend()`，不证明已进入任一设备阶段。
2. `enter_state()` 执行 notifier、冻结任务和 `suspend_devices_and_enter()`。
3. `suspend_devices_and_enter()` 先调用 `platform_suspend_begin()`、`suspend_console()`，再于 `:507` 调用 `dpm_suspend_start(PMSG_SUSPEND)`。
4. `drivers/base/power/main.c:1927-1939` 的 `dpm_suspend_start()` 先运行 `dpm_prepare()`，成功后再运行 `dpm_suspend()`。
5. `suspend.c:513-514` 在 `dpm_suspend_start()` 返回后命中 `suspend_test(TEST_DEVICES)`，等待 `pm_test_delay=5` 秒并跳到 `Recover_platform`。
6. `suspend_enter()` 只在 `suspend.c:517` 调用；本模式在此之前已经跳转，所以 `dpm_suspend_late()`、`dpm_suspend_noirq()`、`platform_suspend_prepare*()`、CPU、syscore 和真实睡眠入口均不执行；前述 `platform_suspend_begin()` 和下面的 `platform_recover()` 仍执行。
7. 恢复分支执行 `platform_recover()`，再由 `dpm_resume_end()` 运行普通 `dpm_resume()` 和 `dpm_complete()`。

因此本轮 `devices` 模式实际阶段是：

```text
pm_suspend entry
  -> suspend_prepare/notifier/freeze
  -> platform_suspend_begin/suspend_console
  -> dpm_prepare
  -> dpm_suspend (普通 suspend)
  -> TEST_DEVICES 的 5 秒等待
  -> platform_recover
  -> dpm_resume (普通 resume)
  -> dpm_complete
  -> thaw/finish
```

不执行：

```text
dpm_suspend_late -> dpm_suspend_noirq -> platform_suspend_prepare*/CPU/syscore -> machine suspend
dpm_resume_noirq -> dpm_resume_early
```

### 2.3 r5dpm2 覆盖表

| 区间 | `devices` 是否执行 | r5dpm2 watchdog | 仍不覆盖的边界 |
|---|---:|---|---|
| `pm_suspend()` 到 `dpm_suspend_start()` 之前 | 是 | 无 | notifier、freeze、console、全局锁和 PM core 前置等待 |
| `dpm_prepare()` 的 `wait_for_device_probe()`、`device_block_probing()` | 是 | 无 | 全局 probe 等待 |
| `dpm_prepare()` 的 `dpm_list_mtx` 遍历/重取锁 | 是 | 无 | list mutex 等待和设备间遍历 |
| `device_prepare()` 的 `pm_runtime_get_noresume()` | 是 | 无，发生在新增 set 之前 | runtime PM 前置操作 |
| `device_prepare()` 的 `device_lock()`、callback 选择和 `->prepare()` | 是 | **新增武装** | timer 本身不可达；syscore 提前返回不武装 |
| `device_suspend()` 的 subordinate wait、`pm_runtime_barrier()` | 是 | 无，位于既有 set 之前 | 子设备完成和 runtime PM 等待 |
| `device_suspend()` 的 `device_lock()`、普通 suspend callback | 是 | **既有武装** | timer 本身不可达 |
| `TEST_DEVICES` 的 5 秒 `mdelay` | 是 | 无 | 固定测试延迟，不属于单设备 callback |
| `device_resume()` 的 superior wait | 是 | 无，位于既有 set 之前 | 父/供应者完成等待 |
| `device_resume()` 的 `device_lock()`、普通 resume callback | 是 | **既有武装** | timer 本身不可达 |
| `device_complete()` 的 `device_lock()`、`->complete()` | 是 | **新增武装** | timer 本身不可达；syscore 提前返回不武装 |
| `device_suspend_late()` | 否 | 无 | 本轮不可达且无新增 hunk |
| `device_suspend_noirq()` callback | 否 | **新增 hunk，但本轮不可达** | subordinate wait 位于 set 前；timer 可达性不作保证 |
| `platform_suspend_begin()`、`platform_recover()` | 是 | 无 | platform 边界不受设备 watchdog 保护 |
| `platform_suspend_prepare*()`、CPU、syscore、真实 suspend | 否 | 无 | 本轮不可达 |

本轮若 watchdog 命中，可从设备名和调用栈定位 prepare/suspend/resume/complete 四阶段之一；若仍硬挂且无 panic，只能把这四个本轮可达的回调阶段降为低可能，候选收敛到 §2.3 的未覆盖等待区或 timer 不可达。noirq hunk 因本轮不可达，不能由该结果排除或确认。

## 3. 内核补丁设计

### 3.1 归档与不可变基线

后续获批后新建归档目录，并按以下布局落位：

```text
.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/
  patches/0001-dpm-watchdog-arm-device-prepare-complete-noirq.patch
  control/
  input/
  source/linux-source-6.12/
  A/
  B/
```

上一轮目录及其源码树只读，不原地打补丁。构建脚本必须：

1. 复算上一轮 source deb SHA `9a5ea91bb8dc78a2a43f190de3c60f44cad91d6164556d0a3676aa1024342303`；
2. 将上一轮已固定签署 key 的源码树复制到新目录，或从同一 source deb 重解包后复制固定 key；
3. 在新目录打补丁前生成内容清单和 `source_tree_before_patch_sha256`；
4. 复算 patch SHA，使用零 fuzz、零 offset 的 fail-closed 应用；任何 `.orig`/`.rej` 或额外改动立即停止；
5. 生成 `source_tree_after_patch_sha256`；前后清单必须只显示 `drivers/base/power/main.c` 改变；
6. A/B 共用该只读 patched source，分别使用独立 `O=` 输出目录，避免源码绝对路径造成 A/B 漂移。

树 hash 算法必须在构建脚本中固定：相对路径、对象类型、mode、文件内容 SHA 或 symlink target 组成按字节序排序的 manifest，再对 manifest 做 SHA-256。前后和 A/B 必须使用同一算法并保存 manifest。

### 3.2 functional hunk 1：`device_prepare()`

基线函数位于 `drivers/base/power/main.c:1796-1855`。预期语义是：

```diff
@@ -1797,6 +1800,7 @@
 {
     int (*callback)(struct device *) = NULL;
     int ret = 0;
+    DECLARE_DPM_WATCHDOG_ON_STACK(wd);

@@ -1809,6 +1813,7 @@
     if (dev->power.syscore)
         return 0;

+    dpm_watchdog_set(&wd, dev);
     device_lock(dev);

@@ -1833,6 +1838,7 @@
 unlock:
     device_unlock(dev);
+    dpm_watchdog_clear(&wd);

     if (ret < 0) {
```

该放置方式与既有普通 suspend/resume 习惯一致：timer 在 `device_lock()` 前启动，在 unlock 后同步清除。它覆盖 callback 为空、`no_pm_callbacks` 和 callback 实际执行三条非 syscore 路径；所有路径都经 `unlock` 清除，不新增返回值或错误映射。`pm_runtime_get_noresume()` 与 syscore 早退仍在覆盖外。

### 3.3 functional hunk 2：`device_complete()`

基线函数位于 `drivers/base/power/main.c:1073-1114`。实际补丁内容：

```diff
@@ -1076,10 +1076,12 @@
 {
     void (*callback)(struct device *) = NULL;
     const char *info = NULL;
+    DECLARE_DPM_WATCHDOG_ON_STACK(wd);

     if (dev->power.syscore)
         goto out;

+    dpm_watchdog_set(&wd, dev);
     device_lock(dev);

@@ -1107,6 +1109,7 @@
     }

     device_unlock(dev);
+    dpm_watchdog_clear(&wd);

 out:
     pm_runtime_put(dev);
```

`dpm_complete()` 在本模式恢复分支执行（§2.2 第 7 条），因此该 hunk 本轮会被动态验证。syscore 早退发生在 set 之前；其余路径统一经 unlock 后 clear，无新增分支或返回值。注意 6.12.101 的 `dpm_watchdog_clear` 签名只有一个参数（main.c:548），既有调用点 994/1710 均为 `dpm_watchdog_clear(&wd);`，补丁必须与之严格一致，不得写成带 `false` 参数的旧内核形式。

### 3.4 functional hunk 3：`device_suspend_noirq()`

基线函数位于 `drivers/base/power/main.c:1219-1289`。预期语义是：

```diff
 static int device_suspend_noirq(struct device *dev, pm_message_t state, bool async)
 {
     pm_callback_t callback = NULL;
     const char *info = NULL;
     int error = 0;
+    DECLARE_DPM_WATCHDOG_ON_STACK(wd);
     ...
 Run:
+    dpm_watchdog_set(&wd, dev);
     error = dpm_run_callback(callback, dev, state, info);
+    dpm_watchdog_clear(&wd);
     if (error) {
```

`dpm_wait_for_subordinate()`、`async_error`、syscore/direct-complete 和 skip 判断仍发生在 set 前。进入 `Run` 后 set/callback/clear 之间不增加分支；callback 为 `NULL` 时立即返回并清除 timer。`dpm_suspend_noirq()` 在此之前调用 `suspend_device_irqs()`，所以未来即使进入 noirq，timer 是否可达仍须按实际现场解释。

本 hunk 在唯一 `pm_test=devices` 轮不可达，只接受源码、编译和产物级证明；不得把本轮无 timeout 写成 noirq 排除证据。

### 3.5 补丁静态门禁

补丁评审必须证明：

1. 只有 `drivers/base/power/main.c` 改变；
2. `dpm_watchdog_set()` 源码调用点由 2 增为 5，且新增点只在上述三个函数；
3. 每个新增 set 都有同一栈对象上的 clear，所有已武装的非 panic 返回路径均清除；
4. callback 调用顺序、返回值、错误分支、async completion、list 移动和 PM 状态位逐字保持；
5. 不改头文件、导出符号、结构布局、KABI、fantgpu 源码或闭源对象；
6. 编译后的 `drivers/base/power/main.o`/`vmlinux` 通过 `objdump -drS` 与 DWARF 行号确认新增 timer set/clear 进入 `device_prepare`（可能被内联至 `dpm_prepare`）、`device_complete`（可能被内联至 `dpm_complete`）和 `device_suspend_noirq`；不得只检查源码文本；
7. handler 符号与 `DPM device timeout`、`unrecoverable failure` 字符串仍存在。

## 4. r5dpm2 可复现构建

### 4.1 配置

诊断 release 固定为：

```text
6.12.101-r5dpm2
```

相对已规范化基线只允许三项诊断语义配置：

```diff
-CONFIG_LOCALVERSION=""
+CONFIG_LOCALVERSION="-r5dpm2"
-# CONFIG_DPM_WATCHDOG is not set
+CONFIG_DPM_WATCHDOG=y
+CONFIG_DPM_WATCHDOG_TIMEOUT=20
```

沿用上一轮已裁定的签署规范化：

```text
CONFIG_MODULE_SIG_ALL=y
CONFIG_MODULE_SIG_KEY="certs/r5-signing-key.pem"
signing_key_sha256=221482be8c19cef56517dcbe5819f17e9b6493019a7f0df281052d299801e0b7
```

固定 key 只为 A/B 字节一致；`CONFIG_MODULE_SIG_FORCE` 仍未启用，Secure Boot 仍关闭，不改变 R5 语义。`scripts/diffconfig` 必须同时保存“原始 `/boot` 配置到 normalized baseline”和“normalized baseline 到 r5dpm2”两层差异；任何未裁定差异停止构建。

`CONFIG_DPM_WATCHDOG=y`、`CONFIG_DPM_WATCHDOG_TIMEOUT=20` 保持不变。cmdline 仍精确包含且各一次：

```text
panic=10 no_console_suspend ignore_loglevel
```

不改变 `pm_async`、`pm_test_delay`、pstore console 或其他 PM 配置。

### 4.2 构建确定性

直接沿用已通过的 r5dpm1 流水线和依赖，不引入新构建系统：

```text
ARCH=x86_64
KBUILD_BUILD_USER=codex-r5
KBUILD_BUILD_HOST=diagnostic
KBUILD_BUILD_TIMESTAMP=2026-09-18T00:00:00Z
KBUILD_BUILD_VERSION=1
SOURCE_DATE_EPOCH=1789689600
KDEB_PKGVERSION=6.12.101-r5dpm2-1
debug-prefix-map=/build/r5dpm
```

A/B 使用独立输出目录和同一 patched source。所有生成的 `.deb` 以及解包后的 `vmlinuz`、`System.map`、`.config`、`Module.symvers` 必须逐字节一致。至少记录：

```text
source_deb_sha256
patch_sha256
source_tree_before_patch_sha256
source_tree_after_patch_sha256
baseline_config_sha256
normalized_baseline_sha256
diagnostic_config_sha256
config_delta
signing_key_sha256
image/headers/debug/libc_deb_sha256_A/B
vmlinuz/System.map/.config/Module.symvers_sha256_A/B
```

包名、release 和安装路径必须只使用 `r5dpm2`，不得覆盖 `6.12.101-r5dpm1` 或原 `6.12.101+deb13-amd64`。构建批只构建和验收，不安装。

## 5. 模块、安装与启动安全

安装批另行放行。安装 image/headers 后，对 `6.12.101-r5dpm2` 单独执行 i6 DKMS build/install、`depmod` 和 initramfs 更新；`modinfo -k 6.12.101-r5dpm2 fantgpu` 的 vermagic 必须匹配。原内核、r5dpm1、r5dpm2 三套 image/modules 可以并存，不删除任何既有 DKMS 实例。

安装脚本沿用上一轮精确 GRUB ID 解析：

1. 安装前备份 `/etc/default/grub`、`grub.cfg`、`grubenv` 并记录 SHA；
2. 从生成后的 `grub.cfg` 精确解析原 `6.12.101+deb13-amd64` 与 `6.12.101-r5dpm2` 的唯一复合 ID；模糊、缺失或重复立即停止；
3. 临时设置 `GRUB_DEFAULT=saved`；
4. `grub-set-default` 永久安全项只能指原 `6.12.101+deb13-amd64`；
5. `grub-reboot` 只把下一次启动指向 r5dpm2；
6. `grub-editenv list` 必须逐项读回 `saved_entry` 和 `next_entry`。

安装完成后停止，不自动重启。首次启动、测试启动和恢复分别放行。

## 6. 五阶段执行脚本

后续脚本只能从步骤 7 最终版复制后做 r5dpm2 路径/版本替换和本节增量，不从旧草稿重写。基线 SHA：

```text
d9bf0bfe4c4692c16bc20405b915ad65f66376effe7e09170bb1479eec7f40a6  10-prepare-arm.sh
2b1537b9479a243bf47be1243bc62171284b57d10eb13550210f816e150df035  20-health-local.sh
6d3ad1479568ee880bb3a591fc9d9a53a859ae6e6f6d9e3157060dd9a5f23da3  30-trigger.sh
e1897b54b6280ee04d7fea2d91c03a0917baf5de3bda35210032b803ab5c646a  40-collect.sh
b92512cfb68c30f502769c16e18302439c788826727f1f38b46296f52b120697  50-restore-grub.sh
```

### 6.1 `10-prepare-arm`

在原内核执行，可从 dwm、SSH 或 TTY 启动；不运行任何 PM 测试。它必须：

- 验证当前内核为 `6.12.101+deb13-amd64`、i6 完整、Driver/Firmware、DRM 节点、`pm_test=none`、`pm_debug_messages=0`；
- 验证 r5dpm2 image/modules/DKMS/initramfs、安装批 SHA 和 GRUB ID 证据；
- 验证 `GRUB_DEFAULT=saved`、saved entry 为原内核复合 ID、`next_entry` 为空、三个 cmdline 参数各一次；
- 保存 prepare boot ID、pstore 前清单并 `sync`；
- 仅调用一次 `grub-reboot <r5dpm2-id>`，读回 next entry 后停止。

### 6.2 `20-health-local`

只允许 r5dpm2 的本地物理 TTY。TTY 门必须使用已实机验证的最终实现：stdin/stdout 均为 TTY，且 `loginctl show-session self` 满足 `TTY=ttyN`、`Remote=no`、`Type=tty`，同时 `SSH_CONNECTION/SSH_CLIENT/SSH_TTY` 全空。不得依赖 sudo 的 `/dev/pts/*` 或缺失的 `SUDO_TTY`。

健康门必须断言：

- 当前 boot ID 同时不同于 prepare boot ID 和 r5dpm2 首启健康 boot ID；
- kernel/cmdline/config/timeout/pstore 全部匹配；
- fantgpu module 路径、vermagic、srcversion、绑定、Driver/Firmware、DRM/fb 节点和运行健康通过；
- `pm_test=none`、`pm_debug_messages=0`、X/dwm 不存在；
- saved fallback 未变、one-shot next entry 已消费；
- 保存 `/sys/fs/pstore` 与 `/var/lib/systemd/pstore` 的 TSV（base、相对路径、SHA、字节数）并 `sync`。

任一检查失败输出 `NOT_RUN` 并停批，不得执行 30。

### 6.3 `30-trigger`

只允许与 20 相同 boot 的本地物理 TTY；单发哨兵存在即拒绝。固定唯一 unit：

```text
r5dpm2-step7-trigger.service
```

使用 `systemd-run --collect --wait`、`Type=oneshot`、`TimeoutStartSec=infinity`，瞬态单元只执行一次：

```text
echo devices > /sys/power/pm_test
echo mem > /sys/power/state
```

启动前证据、提示文字和哨兵必须 `sync`。若 `systemd-run` 返回非零，写 `trigger=FAILED_OR_UNVERIFIED`、保存 unit Result/ExecMainStatus、尽力复位 PM 状态并停批；该分支严禁声称 `NOT_REPRODUCED`，也不得重跑。正常返回才写 `trigger=RETURNED` 并复位。

挂死时等待 watchdog/panic 自动重启；60 秒仍无自动重启只允许长按电源键冷启动一次。操作者不抄录屏幕内容。

### 6.4 `40-collect`

回到原内核后在本地物理 TTY 运行一次，模式只能是：

```text
auto-reboot | cold-reboot | returned-reboot
```

收集器必须：

- pstore 复制是守卫通过后的第一项取证动作，随后立即 `sync`；
- 对 before/after TSV 做 C locale 排序与 hash 差分，提取新增记录；
- 从健康结果读取测试 boot ID，并用 `${test_boot//-/}` 去连字符后传给 `journalctl -b`；
- 保存 kernel/full journal、pstore 原文、恢复 dmesg、健康结果和 SHA manifest；
- 原始输出只能写 `r5_dpm_watchdog_capture=UNCLASSIFIED`；
- 三种 recovery mode 与 returned marker 必须互斥一致；
- 最后复位并确认 `pm_test=none`、`pm_debug_messages=0`。

### 6.5 `50-restore-grub`

回到原内核、40 PASS 且 dsh 单独放行后，在本地物理 TTY 恢复。沿用安装备份 SHA 门、文件整行相等、grubenv 字节级恢复、`next_entry` 为空和 cmdline 中 `panic=10` token 计数为 0。

本轮新增“配置与启动行为双恢复”门禁。安装前必须从备份 `grub.cfg` 保存：

```text
preinstall_first_top_level_menuentry_title
preinstall_first_top_level_menuentry_id
preinstall_first_top_level_linux_image
preinstall_effective_default_title/id/linux_image
```

解析器沿用安装脚本的严格 Python 标准库模式，必须理解当前原配置的 `GRUB_DEFAULT=0`；若将来值不是受支持的数字索引或 `saved`，则 fail-closed，不猜测。恢复后运行 `update-grub`，重新解析并断言：

1. `/etc/default/grub` 与安装前备份整文件 SHA 一致；
2. `grubenv` 与安装前备份整文件 SHA 一致，`next_entry` 为空；
3. 第一顶层菜单项的 title、ID 和实际 `linux /boot/vmlinuz-*` 与安装前记录一致；
4. 依据恢复后的 `GRUB_DEFAULT`/saved entry 解析出的有效默认项与安装前记录一致；
5. 默认项不是 r5dpm1/r5dpm2，除非它本来就是安装前记录（当前不是）；
6. 原 `6.12.101+deb13-amd64` 手工回退项仍唯一存在。

只恢复配置但默认启动解析不一致时，输出 restore FAIL，明确提示下次开机手选原内核；不得输出“启动行为已恢复”。r5dpm1/r5dpm2 包默认保留，证据闭合后由 dsh 决定处置。

## 7. 判定与证据

### 7.1 复核判定

运行时收集器不自动解释文本，只输出 `UNCLASSIFIED`。qoder/dsh 复核 pstore、测试 boot journal、触发边界和恢复模式后落正式结果：

| 现场 | `r5_dpm_watchdog_capture_reviewed` | 归因上限 |
|---|---|---|
| timeout/panic 含设备名和可读 stack | `LOCATED` | 按栈帧定位到 prepare/suspend/resume/complete 四个回调阶段之一；最高 vendor frame 为候选挂点 |
| timeout/panic 存在但 stack 截断 | `PARTIAL` | 设备可定位，函数或阶段不可定位 |
| `pm_test` 正常返回 | `NOT_REPRODUCED` | 只说明本轮未复现；`R5` 仍为 FAIL |
| 再次硬挂且无 DPM panic | `OUTSIDE_COVERAGE` | §2.3 列举的未覆盖等待区或 timer 不可达；四个本轮可达回调阶段降为低可能，noirq 不作判断 |
| r5dpm2 健康门失败、未触发 | `NOT_RUN` | 构建/兼容/门禁问题 |

`systemd-run` 非零或无法证明写入边界时，触发状态必须为 `FAILED_OR_UNVERIFIED`；不填写 `NOT_REPRODUCED`，也不重复实验。

### 7.2 证据字段

原始源码、补丁、构建产物、pstore 和 journal 全部进入 ignored `.runtime-archive`。Git 只收脱敏摘要与原始文件 SHA/字节数/归档相对路径。至少记录：

```text
r5_investigation_stage=dpm_prepare_watchdog_diagnostic_kernel
r5_diagnostic_kernel=6.12.101-r5dpm2
r5_dpm_watchdog_timeout=20
r5_patch_sha256=<sha256>
r5_source_tree_before_patch_sha256=<sha256>
r5_source_tree_after_patch_sha256=<sha256>
r5_watchdog_sites=device_prepare,device_complete,device_suspend_noirq,device_suspend,device_resume
r5_runtime_reachable_new_sites=device_prepare,device_complete
r5_runtime_unreachable_new_site=device_suspend_noirq
r5_dpm_timeout_device=<name-or-UNAVAILABLE>
r5_detector_reported_driver=<name-or-UNAVAILABLE>
r5_detector_reported_task=<comm/pid-or-UNAVAILABLE>
r5_pstore_stack_top=<symbol-or-UNAVAILABLE>
r5_highest_vendor_frame=<symbol-or-UNAVAILABLE>
r5_dpm_watchdog_capture=UNCLASSIFIED
r5_dpm_watchdog_capture_reviewed=LOCATED|PARTIAL|NOT_REPRODUCED|OUTSIDE_COVERAGE|NOT_RUN
r5_root_cause=<candidate-or-unresolved>
r5_validation_status=FAIL
```

每次脚本、补丁或设计变化都必须重出全量 SHA；不得混用旧评审哈希。归档摘要必须可逐行复算。allowlist 仍只由 dsh 在 commit-time 正式重生成。

## 8. 回滚

### 8.1 内核回退

测试期间 saved default 永远指向原 `6.12.101+deb13-amd64`。r5dpm2 起不来时，下一次应自动回原内核；未自动回时，在 GRUB 的 Advanced options 手选 `6.12.101+deb13-amd64`。回去后先确认：

```text
uname -r=6.12.101+deb13-amd64
Driver/Firmware=OK
pm_test=[none]
pm_debug_messages=0
```

r5dpm1 与 r5dpm2 包都保留，不在恢复现场卸载；最终处置由 dsh 在证据闭合后决定。

### 8.2 4.0.2-i3 用户恢复卡

仅当原内核上的 i6 驱动也异常时使用；不预先执行：

```bash
cd ~/src/innogpu-fh2m-debian-trixie
printf '%s  %s\n' 177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662 build/innogpu-fh2m-trixie_4.0.2-i3.deb | sha256sum -c -
# 校验不通过立即停止，不执行后续卸载/安装命令。
sudo dpkg --remove fantgpu-fh2m-trixie || sudo dpkg --purge --force-all fantgpu-fh2m-trixie
sudo dpkg -i build/innogpu-fh2m-trixie_4.0.2-i3.deb
printf '%s\n' none | sudo tee /sys/power/pm_test
printf '%s\n' 0 | sudo tee /sys/power/pm_debug_messages
sudo reboot
```

重启后：

```bash
cd ~/src/innogpu-fh2m-debian-trixie
scripts/verify-install-status.sh --require-reboot 4.0.2-i3
startx
```

如果连原内核也无法自动进入，先在 GRUB 手选 `6.12.101+deb13-amd64`，再执行恢复卡。

## 9. 分级放行流程

1. codex 只提交本设计稿给 qoder 初审，不构建、不安装、不运行；
2. qoder 初审源码阶段图、三个 functional hunk（prepare+complete+noirq）、pm_test=devices 阶段结论与覆盖表、构建确定性和恢复语义；
3. dsh 终审设计并执行设计提交；
4. dsh 明示放行后，codex 才生成 patch、构建/验收脚本，在新 ignored 归档完成 A/B 双构建，不安装；
5. qoder 初审 patch、前后树 hash、config diff、反汇编和 A/B 产物；dsh 终审构建证据；
6. dsh 单独放行安装；安装只安装 image/headers、目标 DKMS、initramfs 和 GRUB 配置，不自动重启；
7. dsh 单独放行 r5dpm2 首启健康门；失败则 `NOT_RUN`，不得触发；
8. 回原内核后由 10 重臂，测试启动中 20 在本地 TTY 复跑正式健康门；
9. dsh 最后单独放行唯一一次 `pm_test=devices`；用户本地在场，完成 30、恢复和 40；
10. qoder/dsh 复核原始证据并落 `capture_reviewed`，随后才放行 50 恢复 GRUB；
11. 任何设计、patch 或脚本改动都使既有评审哈希失效，必须全量重算并重新初审。

任何阶段 PASS 都不把 R5 提升为 PASS，也不解除 `U1/U2`、`validation-results`、签发或 tag 冻结。

## 10. 本设计批自检

本批只允许新增本文档。提交前：

```text
git diff --check
```

本文档不交付可执行脚本，因此本批没有 `bash -n` 对象。不得修改上一轮设计、五脚本、证据文件、源码树或 allowlist；不 commit，由 dsh 终审后提交。
