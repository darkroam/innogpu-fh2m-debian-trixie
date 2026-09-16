# R5 本机 watchdog 抓现场演练设计

- 状态：**待 qoder 初审、dsh 终审；仅设计，不执行**
- 日期：2026-09-16
- 适用版本：已安装并运行的 `5.0.0-i6` / `6.12.101+deb13-amd64`
- 目标：在没有串口、第二台机器和 SSH 的条件下，用本机 watchdog、自动重启和 EFI pstore 捕获 R5 `pm_test=devices` 挂点
- 冻结：`R5=FAIL`、`U1/U2`、`validation-results`、签发和 tag 均保持冻结

## 1. 证据与边界

### 1.1 当前状态复核

本稿起草前只读核对得到：

```text
kernel=6.12.101+deb13-amd64
package=fantgpu-fh2m-trixie 5.0.0-i6 installed
Driver Status=OK
Firmware Status=OK
pm_test=[none] core processors platform devices freezer
pm_debug_messages=0
```

运行树身份沿用已批准的 i6 指纹；PCI suspend 的 `pci-entry` 中止轮仍挂死，且 `r5-stop-i6-pci-entry-all-markers.txt` 为空。因此本轮不再重复 stop-stage，不再执行 `pre-power-sleep`，也不执行真实 `mem` suspend。

### 1.2 EFI pstore 能力边界

既有证据 `docs/planning/evidence/o-stage/runtime-5.0.0-i3/r5-pstore-efi-check.txt` 已记录：

```text
pstore_backend=efi_pstore
efi_pstore_loaded=yes
efi_pstore_disabled=no
efi_pstore_record_size=1024
config_pstore_console=not_set
post_hang_pstore_files=0
```

因此：

- EFI 后端可保存支持的 panic/oops kmsg 记录，但不能接收普通 `dev_notice`/console marker；
- 本轮依赖 watchdog **触发 panic**，不依赖 pstore console frontend；
- 每个 EFI record 只有 1024 字节，不能声称能保存完整调用栈；收集器必须保存所有 record 原件并在摘要中标出截断状态；
- pstore 为空只能判为“本轮没有获得 pstore 证据”，不能倒推驱动未挂死。

### 1.3 调查结论边界

本轮最多定位到：

1. softlockup/hardlockup panic 或窗口外 hung-task panic 的 blocked task 栈顶/RIP；
2. 栈中的最高 F 驱动函数或 PM 子设备函数；
3. watchdog 未触发时的失败分支；
4. 若三类本机检测器均不可达，明确记录 `hang_class_unknown`，不把空 pstore 解释为配置失败。

这不能证明栈顶函数就是根因。若栈顶为 scheduler、workqueue 或通用等待函数，记录完整可见栈和最高 vendor frame，结论仍为 `CANDIDATE`，不改 `R5=FAIL`。

## 2. 配置设计

配置由受审的本机脚本完成，禁止手工逐行输入。脚本必须先保存以下文件的原文、SHA-256、字节数和文件模式，再变更：

```text
/etc/sysctl.d/99-r5-watchdog.conf
/etc/default/grub
/boot/grub/grub.cfg
```

备份原件放入 Git 忽略的 `.runtime-archive/runtime-5.0.0-i6/`，不得加入 Git。配置脚本只允许添加本轮拥有的 sysctl 文件，不覆盖其他 sysctl 文件；GRUB 只在 `GRUB_CMDLINE_LINUX_DEFAULT` 中添加一个 `panic=10` token，已存在且值相同则幂等跳过。

### 2.1 watchdog sysctl

写入 `/etc/sysctl.d/99-r5-watchdog.conf`：

```text
kernel.hung_task_panic=1
kernel.hung_task_timeout_secs=30
kernel.hardlockup_panic=1
kernel.softlockup_panic=1
```

立即加载并逐项验证：

```bash
sudo sysctl --system
sudo sysctl -n kernel.hung_task_panic
sudo sysctl -n kernel.hung_task_timeout_secs
sudo sysctl -n kernel.hardlockup_panic
sudo sysctl -n kernel.softlockup_panic
```

预期输出依次为：

```text
1
30
1
1
```

任一键不存在、写入失败或读回值不等于预期，脚本输出 `watchdog_config=FAIL`，不进入下一步。

额外记录运行时 NMI watchdog 状态，优先使用本机内核接口：

```bash
if [[ -r /proc/sys/kernel/nmi_watchdog ]]; then
    cat /proc/sys/kernel/nmi_watchdog
elif [[ -r /sys/module/lockup_detector/parameters/nmi_watchdog ]]; then
    cat /sys/module/lockup_detector/parameters/nmi_watchdog
else
    echo unavailable
fi
```

`/sys/module/lockup_detector/parameters/nmi_watchdog` 仅作可选交叉核对。读回值写入 `r5_nmi_watchdog`；只有值为启用状态时，hardlockup 路径才记为可观测。两个接口都缺失或值为 `0` 只能记为 `UNVERIFIED`，不能由 `kernel.hardlockup_panic=1` 反推 NMI 检测器已运行。

验证内核确实提供 hardlockup 检测能力：

```bash
grep -E '^(CONFIG_HARDLOCKUP_DETECTOR|CONFIG_HARDLOCKUP_DETECTOR_PERF)=' \
  "/boot/config-$(uname -r)"
```

至少应看到一个有效的 `=y`。如果只有 `=m`、未设置或文件不存在，仍可保留 `kernel.hardlockup_panic=1` 作为备用配置，但结果必须标记 `hardlockup_detector=UNVERIFIED`；不能把它当作 NMI 保护已生效。

### 2.2 panic 自动重启

配置脚本执行以下逻辑，而不是直接覆盖整个 GRUB 配置：

1. 备份 `/etc/default/grub` 和当前 `/boot/grub/grub.cfg`；
2. 在 `GRUB_CMDLINE_LINUX_DEFAULT` 的引号内容中追加 `panic=10`；
3. 运行 `grub-mkconfig -o /boot/grub/grub.cfg`；
4. 不在配置阶段重启；
5. 由 post-config 验证脚本检查生成文件包含一个独立的 `panic=10` token；
6. 唯一一次测试重启后，第一项验证必须是 `/proc/cmdline`。

生效验证命令：

```bash
grep -oE '(^|[[:space:]])panic=10([[:space:]]|$)' /proc/cmdline
```

预期输出包含 `panic=10`。若缺失，结果为 `cmdline_panic=FAIL`，不得触发 `pm_test=devices`；需按失败恢复卡回退配置。

现有 cmdline 中的 `no_console_suspend ignore_loglevel` 必须保留；脚本不得删除或重排其他启动参数。

## 3. 单次执行协议

### 3.1 轮次文件

执行目录固定为：

```text
.runtime-archive/runtime-5.0.0-i6/
```

脚本不得覆盖旧文件，采用新轮次目录或带 boot ID 的文件名。至少生成：

```text
r5-watchdog-config.txt
r5-watchdog-arm.txt
r5-watchdog-pm-test-run.txt
r5-watchdog-pstore.txt
r5-watchdog-result.txt
r5-watchdog-sha256.txt
```

每个文件记录 `boot_id`、时间戳、内核、包版本、配置读回值、`pm_test` 前后值、返回码和脚本版本。所有文件写入后执行 `sync`；触发前必须再次 `sync`，确保 arm 文件和恢复卡已经落盘。

### 3.2 触发步骤

收集器和触发器必须由 root 的独立 systemd service 运行，不能依赖 SSH、DWM、X、前台终端或用户 shell。用户只需要在本地看现场并在自动重启后配合执行收集入口。

触发器的唯一一次内核操作等价于：

```bash
printf '%s\n' devices | sudo tee /sys/power/pm_test
printf '%s\n' mem | sudo tee /sys/power/state
```

实际脚本必须在写入前验证：

```text
pm_test=[none] ...
pm_debug_messages=0
Driver/Firmware=OK
PCI driver link=fant-drv
DRM card0/renderD128/fb0=present
cmdline_panic=PASS
watchdog_config=PASS
```

若任何前置失败，脚本只记录 `arm=FAIL`，不写 `/sys/power/pm_test`。`platform` 级别、真实 `mem` suspend、重复 devices 轮次和 stop-stage 重跑全部禁止。

### 3.3 检测器可达性与预期现场

正常返回时，触发器记录 RC、完整 `devices suspend/resume` 周期和收尾状态；本轮目标不是正常返回，结果仍不得提升为 PASS。

v6.12 的 PM core 在 `PM_SUSPEND_PREPARE` 通知阶段先将 `hung_detector_suspended=true`，随后才进入 freeze/device suspend；正常返回才会发送 `PM_POST_SUSPEND` 恢复检测。因此 `pm_test=devices` 的挂起窗口内，`khungtaskd` 会跳过检查，`kernel.hung_task_timeout_secs=30` **不能触发本轮 hung-task panic**。hung-task 仅保留为窗口外的退化网，不能作为本轮自动重启的主机制。

本轮实际可达性如下：

| 挂死类型 | 本机检测器 | 预期 | 证据口径 |
|---|---|---|---|
| IRQ 开启、内核循环超过阈值 | softlockup | `softlockup_panic=1` 后 panic，`panic=10` 自动重启 | pstore 可能有 panic kmsg 和调用栈 |
| IRQ 关闭、CPU 长时间无进展 | hardlockup | 仅当 NMI watchdog 运行且 `hardlockup_panic=1` 时可能 panic | `r5_nmi_watchdog` 必须独立记录 |
| 纯 D 态睡眠/等待 | 无本轮 PM 内 hung-task 检查 | watchdog 可能不触发，系统保持挂死 | `NO_REBOOT + pstore 空` 是预期退化结果，不是配置失败 |
| PM 窗口外的 D 态任务 | hung-task | 可由 30 秒超时触发，非本轮主证据 | 只作为退化网记录 |

因此不能用“30 秒后必重启”描述本轮；`panic=10` 只负责已发生 panic 后的重启，不负责制造 panic。watchdog 也不能中止已经卡在不可抢占路径中的线程。

不允许等待 SSH、按屏幕判断、重复写 sysfs 或执行软关机。若 watchdog 未自动重启，用户只按既定人工恢复卡处理；不得在同一 boot 再次尝试。

### 3.4 可选 SysRq 人工兜底（待 dsh 决策）

这不是自动协议，也不改变“用户只看、脚本落盘”的主流程。上游 v6.12 的 `w`（show blocked tasks）和 `c`（crash）均由 `SYSRQ_ENABLE_DUMP=0x8` 门控。只有 dsh 明示批准、控制台仍能接收键盘且 `kernel.sysrq` 掩码包含 `0x8` 时，才可由现场用户执行：

```bash
sysrq_mask=$(cat /proc/sys/kernel/sysrq)
(( (sysrq_mask & 0x8) == 0x8 ))
```

随后按 `Alt+SysRq+w` 留下 blocked-task 信息，再按 `Alt+SysRq+c` 触发 panic，由 `panic=10` 自动重启。当前本机历史读数为 `438`（`0x1b6`），不含 `0x8`，因此该兜底默认不可用。除非 dsh 另行批准将 `kernel.sysrq` 持久配置为 `1` 或其他包含 `0x8` 的掩码并读回确认，否则不得认证这条路径。若掩码不满足、控制台无响应或用户无法确认顺序，跳过兜底并按 `NO_REBOOT` 分支恢复。

## 4. 重启后第一动作与自动收集

启动收集器必须先处理 pstore，再做健康检查、journal 或桌面恢复。顺序固定：

1. 读取并复制 `/sys/fs/pstore/*` 到本轮归档；目录为空也要写 `pstore_files=0`；
2. 对每个原件计算 SHA-256、字节数和复制结果，执行 `sync`；
3. 记录当前 boot ID、上一轮 arm 文件中的 test boot ID 和二者是否不同；
4. 生成 `r5-watchdog-pstore.txt` 的脱敏摘要；
5. 最后才读取 runtime health、journal 和 sysfs 状态，并执行 watchdog 配置恢复。

不得先运行会清空 pstore 的工具，也不得先启动 DWM。原始 pstore 文件保留在 ignored 归档；Git 只接受脱敏摘要，不接受原始 panic kmsg。

### 4.1 栈提取规则

收集器不修改原始文件，只生成摘要：

- 优先匹配 `RIP:`、`PC:`、`Call Trace:`、`<symbol+offset>` 行；
- 记录第一处可解析的当前指令符号；
- 继续记录调用栈中最高的 `fantgpu`/`fantdpu`/`ft_`/`PVRSRV`/`drm_` vendor frame；
- 同时保留被检测器报告的任务 comm、PID、状态（含 D 态）和所有可见调用栈行；本轮 PM 窗口内 hung-task 检测被暂停时，该字段允许为 `UNAVAILABLE`；
- 如果 1024 字节边界截断，写 `record_truncated=yes`，不得用相邻 record 拼接成未经验证的完整栈。

### 4.2 判定表

| 条件 | 判定 | 后续 |
|---|---|---|
| boot ID 改变，pstore 有文件，RIP/Call Trace 栈顶可读 | `WATCHDOG_CAPTURE=LOCATED` | 停批，交 qoder/dsh 做函数归因 |
| boot ID 改变，pstore 有文件但只有截断头/无栈顶 | `WATCHDOG_CAPTURE=PARTIAL` | 不重复本轮；评估 ramoops 或减少输出后再裁决 |
| boot ID 改变，pstore 为空 | `WATCHDOG_CAPTURE=EMPTY` | 分支记录 EFI 未写、record 被截断/覆盖或 panic 未留下记录；不声称未挂死 |
| boot ID 未改变且本机仍可操作 | `WATCHDOG_TRIGGER=NOT_REACHED_OR_RETURNED` | 记录 arm/trigger 状态，不追加挂起级别 |
| boot ID 未改变且系统硬挂 | `WATCHDOG_TRIGGER=NO_REBOOT` | 首因按检测器矩阵记录：纯 D 态时 hung-task 在 PM 窗口内被暂停；另记录 hardlockup/softlockup 是否具备运行条件；按恢复卡处理 |
| 当前 boot 健康恢复失败 | `RECOVERY=FAIL` | 立即进入 4.0.2-i3 回退卡，不恢复桌面 |

无论哪一行，`r5_validation_status=FAIL` 保持不变。

## 5. EFI record 截断与 ramoops 退化方案

### 5.1 EFI 1024 字节处理

当前不能运行时扩大 `efi_pstore_record_size=1024`。因此采用以下最小输出策略：

1. 只启用本任务要求的四个 sysctl，不打开额外的全 CPU hung-task backtrace；
2. 保留现有 `ignore_loglevel`，让 panic 关键信息进入 kmsg；
3. pstore 收集器复制全部 record，不只取一个文件；
4. 解析每个 record 的 `RIP`、`Call Trace` 和 vendor frame；
5. raw record、摘要和截断状态分别保存并计算 hash。

若仍只有 panic 头而无栈顶，结果为 `PARTIAL`，不能通过增加等待时间解决，也不能重复 pm_test 作为补救。

### 5.2 ramoops 备选

只有 EFI 记录反复为空或无栈顶、且 dsh 另行批准时，才评估 ramoops。它仍是纯本机方案，但需要一次可用的 RAM 保留区配置和一次重启，不能在本轮未批准时写入内核启动参数。

启用条件：

- `/proc/iomem` 有明确未占用的保留物理区；
- 由 dsh 确认 `memmap=持久区大小$物理地址` 不覆盖 RAM、ACPI、GPU BAR 或 initramfs；
- 内核提供 `ramoops` 模块/内建支持；
- 配置 `ramoops.mem_address`、`ramoops.mem_size`、`ramoops.record_size` 前完成冲突检查和备份；
- 先做普通 panic/测试记录验证，再决定是否进入唯一一次 R5 观测轮。

回退条件：任何物理区冲突、内核不接受参数、启动后 `pstore` 后端不变、或需要第二次不可控重启时，放弃 ramoops，恢复 GRUB/sysctl 原件并维持 `R5=FAIL`。不把 ramoops 设计成当前轮次的隐含后门。

## 6. 用户恢复卡

以下命令卡必须原样写入执行脚本的失败提示和设计产物。用户只在 watchdog 未自动恢复、当前 i6 无法正常启动或 dsh 明示回退时执行；不要在 pstore 尚未复制前操作。

### 6.1 从当前系统回退 4.0.2-i3

```bash
cd /path/to/innogpu-fh2m-debian-trixie
echo '177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662  build/innogpu-fh2m-trixie_4.0.2-i3.deb' | sha256sum -c -
sudo dpkg --remove fantgpu-fh2m-trixie
sudo dpkg -i build/innogpu-fh2m-trixie_4.0.2-i3.deb
sudo sh -c 'echo none > /sys/power/pm_test; echo 0 > /sys/power/pm_debug_messages'
sudo reboot
```

重启后：

```bash
cd /path/to/innogpu-fh2m-debian-trixie
sudo scripts/verify-install-status.sh --require-reboot 4.0.2-i3
startx
```

预期：`verify-install-status.sh` 返回 PASS，包为 `innogpu-fh2m-trixie 4.0.2-i3`，`Driver/Firmware`、DRM 节点和 `pm_test=none` 正常。若验证失败，不启动 DWM，保留现场交审。

### 6.2 GRUB 备用内核路径

若当前 i6 不能启动：

1. 开机时按住 `Esc`（UEFI）或 `Shift`（Legacy BIOS）进入 GRUB；
2. 选择 `Advanced options for Debian GNU/Linux`；
3. 选择已知能正常启动、且与 `4.0.2-i3` 验证记录对应的备用 `6.12.101+deb13-amd64` 内核条目；
4. 进入系统后先执行 §4 的 pstore 收集入口，再执行 §6.1；
5. 若菜单没有该条目，停止操作并保留启动菜单现场，不猜测其他内核。

备用内核选择只解决“进入可操作系统”的问题，不替代包 SHA 校验和 `--require-reboot` 验证。

## 7. 零手抄和证据规则

脚本必须自动写入以下字段：

```text
r5_investigation_stage=watchdog_local_capture
r5_pm_test_freezer=not_run
r5_pm_test_devices=pass|hang|fail|not_run
r5_pm_test_platform=not_run
r5_gpu_binding=bound|unbound|unknown
r5_desktop_clients=stopped|unknown
r5_watchdog_triggered=yes|no|unknown
r5_hung_task_detector=pm_suspended|active|unknown
r5_softlockup_panic=1|0|unknown
r5_hardlockup_panic=1|0|unknown
r5_nmi_watchdog=enabled|disabled|unavailable|unknown
r5_hang_class=hardlockup|softlockup|d_state|unknown
r5_pstore_backend=efi_pstore|ramoops|none|unknown
r5_pstore_files=0|positive_integer
r5_pstore_stack_top=<raw-or-UNAVAILABLE>
r5_pstore_record_truncated=yes|no|unknown
r5_root_cause=<candidate-or-unresolved>
r5_validation_status=FAIL
```

原始文件只留在 `.runtime-archive/`；脱敏摘要每行包含文件名、SHA-256、字节数、脱敏规则和归档路径。任何主机名、用户路径、网络地址或原始 panic kmsg 不进入 Git。执行前后分别运行隐私扫描；扫描失败则不提交摘要。

本设计不新增驱动源码、补丁、构建产物、ABI 字段或 tag。通过门禁不等于 R5 通过；它只证明设计文档没有破坏仓库审计边界。

## 8. 放行与停止条件

当前只请求以下流程：

1. codex 提交本设计文档；
2. qoder 初审配置语义、pstore 判定和回退卡；
3. dsh 终审并在 commit-time 重生成 allowlist；
4. 批准后另行生成并审查本机控制/收集脚本；
5. 脚本和真机执行必须再次取得 dsh 明示放行，用户在场监督。

以下任一条件立即停批：配置验证失败、`panic=10` 未进入 `/proc/cmdline`、pstore 收集器无法先于其他动作运行、自动重启未发生、恢复状态不健康、或需要第二次重复 `pm_test=devices`。任何停批只增加证据，不解除 R5 冻结。
