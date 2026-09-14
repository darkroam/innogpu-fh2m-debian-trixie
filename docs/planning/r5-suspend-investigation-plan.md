# R5 suspend 故障调查方案（5.0.0-i2）

- 状态：**codex 修订稿，已吸收 qoder 初审并落实 dsh 终裁；阶段 A 已获只读放行，阶段 B/C/D 仍须逐阶段明示放行；真机阶段继续冻结**
- 日期：2026-09-14
- 适用对象：已安装并运行 `5.0.0-i2` 的同一台真机
- 当前结论：`R5=FAIL`；`U1/U2`、`validation-results.json`、签发和 tag 继续冻结
- 角色：codex 负责方案与执行实现，qoder 负责初审，dsh 负责终审并执行提交；真机动作由用户在场监督

## 1. 问题与证据

### 1.1 已知事实

目前已观察到：

1. 真实 `systemctl suspend` 和 `rtcwake -m mem` 各发生一次未恢复的停滞；第二次有 RTC alarm，机器风扇未停止，说明没有完成可确认的 S3 周期。
2. `pm_test=freezer` 通过，约 5 秒自动回弹。
3. `pm_test=devices` 在 `fantgpu` 绑定、桌面运行时挂起；该尝试没有返回。
4. 停止桌面并解绑 `0000:02:00.0` 后，`pm_test=devices` 在约 6 秒内返回 `RC=0`，dmesg 有完整的 devices suspend/resume 周期。
5. 当前系统已恢复：`fantgpu` 绑定、Driver/Firmware OK、`pm_test=none`、`pm_debug_messages=0`。

权威证据目录为 `docs/planning/evidence/o-stage/runtime-5.0.0-i2/`。其中 32 个未跟踪原始运行产物已按 dsh 终裁移至仓库外归档 `~/innogpu-runtime-archive/runtime-5.0.0-i2/`；Git 仅保留 `raw-evidence-archive-summary.txt`（每个原始文件一行，含 SHA-256、字节数、脱敏规则和归档路径）以及 `c3-firmware-abort.txt`、`preinstall-state.txt`、`install-rollback.txt` 等结构化审计记录。

### 1.2 对既有二分的复核

已有结果支持“绑定的 fantgpu 设备路径高度相关”，但还不能写成最终因果闭合，因为对照组同时改变了两个变量：

- 绑定态：桌面/X/DRM 客户端存在，设备绑定；
- 解绑态：桌面会话终止、设备解绑，系统为 headless。

因此解绑态通过可能来自设备解绑，也可能来自释放 X/DRM 客户端后的交互变化。解绑操作本身也破坏了继续观察原显示会话的条件。下一项实验必须保持设备绑定，只去除桌面/DRM 用户态变量。

`pm_test` 结果也不能等同于真实 S3：它用于定位内核 suspend 流程阶段，不能证明平台实际断电或唤醒成功。已有真实 suspend 两次失败足以维持 R5=FAIL。

### 1.3 代码调查面与审查对象

代码审查对象必须是当前运行模块对应的已安装源码树：`/usr/src/fantgpu-fh2m-kernel-2.2`，树 hash 为 `c44ce785…`（O_stage 14 链最终树，且运行时 `dpkg -V` 干净）。仓库中的 `build/r16-fantgpu-deb` 是过期中间产物，不得作为本轮 F suspend 结论依据；其缺少 030-028/`compat_kernel6.h` 等内容，引用行号前必须先核对安装树 hash。

当前运行 `fantgpu.ko` 的符号边界也必须显式记录：

| 符号/代码 | 证据性质 | 本方案允许的结论 |
| --- | --- | --- |
| `fantgpu_device_suspend`、`fantgpu_pmops_suspend` | F 源码，可静态审读 | 可核对入口、调用顺序、可见返回值和 030-028 计数逻辑 |
| `fh2m_hal_dma_suspend` | F 源码 `hal_dma.c`，可静态审读 | 可核对源码实现及其调用约束 |
| `hal_power_sleep`、`hal_power_wakeup`、`hal_check_reg_accessiable`、`hal_pdp_restore_default_cfg` | `fantgpu/fantgpu.o_shipped` 中的预编译实现 | 只能记录符号存在、调用位置和真机行为；不能声称已审计其内部等待/超时 |

该模块由约 121 个源码单元和一个由 Kbuild 从 `fantgpu.o_shipped` COPY 的约 6.9 MiB 预编译对象链接生成。因而“调用方源码可见”与“HAL 内部可证明”必须分栏，不能笼统写成 F 驱动源码已完整覆盖。

F 血统包内源码显示 PCI 电源回调位于 `fantgpu/fantgpu_pci_drv.c`：

- `fantgpu_pmops_suspend()` 调用 `fantgpu_device_suspend(dev, true)`；
- 该函数依次调用 `fh2m_hal_dma_suspend()`、`hal_power_sleep()`、`hal_check_reg_accessiable()`、`disable_irq()`、`hal_pdp_restore_default_cfg()`，随后执行 PCI state 保存、设备禁用和电源状态切换；
- resume 路径执行 PCI 恢复、`hal_pci_irq_resume()`、`enable_irq()`、`pm_resume` 回调和 `hal_power_wakeup()`。

DRM 侧 `fantsrvkm/fantdpu_drm_pm.c` 的 suspend 路径还会执行 CRTC backup、`fantdpu_drm_hibernation()` 和 GEM backup；resume 路径执行 GEM recover、DRM wakeup 和 CRTC restore。预编译 HAL 的内部等待点不可仅由静态源码证明，必须把代码审查结论和真机阶段结果分开记录。

030-028 必须在阶段 A 排在首位：审读 `fantgpu_device_suspend()` 中 `pvr_resume_count` 的 suspend 初始化、`ft_drm.c`/`ft_pm_resume` 中按 `dev_nums` 聚合恢复温控 work 的计数与顺序，以及 `hal.h` 中计数器的布局/初始化。重点检查重复 resume、缺失子设备、`resumed > dev_nums`、并发 work 和错误回滚；030-024（DVFS 门禁）、030-026（`pdp0_crtc.c`，PDP/CRTC 编排）与 030-026-lifecycle（`ft_drm.c`，生命周期编排）随后审读，030-030 只在其影响电源/音频依赖时纳入关联分析。

### 1.4 测试缺口

现有 builder、payload、DRM、DMA-BUF 和普通安装验证不能覆盖 F HAL 在本机 suspend 路径中的运行时等待：HAL 实现在预编译对象中，测试既不能枚举其内部等待点，也没有现成的“设备 suspend 回调已返回”断言。现有 `pm_test` 结果只能把问题定位到 `freezer` 之后的 `devices` 阶段，不能自动断言 `fh2m_hal_dma_suspend()`、`hal_power_sleep()` 或 `hal_pdp_restore_default_cfg()` 的内部完成情况；这正是本调查需要补齐的证据，而不是把已有静态门禁误解释为 suspend 覆盖。

## 2. 调查策略

调查按最小风险和信息增益排序。每个真机实验只做一次；前一级未返回或状态未恢复，不进入后一级。

### 阶段 A：只读静态调查（不改机器）

qoder 初审时先复核：

1. `fantgpu_device_suspend()` 的调用顺序、错误返回和是否存在无超时 HAL 调用；特别检查 `fh2m_hal_dma_suspend`、`hal_power_sleep`、`hal_pdp_restore_default_cfg` 的声明、实现可见性、调用约束，以及调用方是否检查返回值。当前 F 源码中这些调用的内部完成条件不可见，审查记录必须明确标注为未知，而不是假定它们有超时。
2. `fantdpu_drm_pm.c` 的 DRM suspend/resume 调用是否可能等待用户态、持有 DRM/GEM 锁或依赖仍在运行的 framebuffer/X 客户端。
3. 030-024、030-026、030-028 与 030-030 的修改是否改变了 suspend 前置条件、work 计数、DVFS drain、固件/音频状态或错误回滚；不得把“静态顺序正确”写成 HAL 运行时安全已证明。
4. 与 O 血统 4.0.2-i3 对照同名路径，形成差异表；O 的 `60/60` 静态测试和真机 PASS 只能作为对照证据，不能单独证明 F 根因。

静态阶段输出一份审查记录，至少包含安装树 hash、`source_path:line`、符号归属（源码/预编译）、调用顺序、可见等待点、已知超时/无超时点和“可由该证据证明的范围”。若无法取得安装树或 hash 不匹配，阶段 A 输出 `UNVERIFIED`，不得退回审查过期 `build/` 树来补结论。

### 阶段 B：bound + headless 对照（首选真机实验）

目的：保持 `fantgpu` PCI 设备绑定，只移除桌面和 DRM 用户态客户端，消除上一轮二分的混杂变量。

执行原则：

1. 用户必须在本地物理控制台在场，另准备独立 TTY；不要依赖将被终止的桌面会话或 SSH 判断屏幕状态。
2. 本机没有 `display-manager` 服务，不能机械执行 `systemctl stop display-manager`。应先确认当前 `startx→dwm` 会话及其子进程，再按用户确认的会话 ID 终止；终止前保存 X/DRM 节点状态。
3. **保持** `/sys/bus/pci/drivers/fant-drv/0000:02:00.0` 绑定，不执行 unbind，不改模块参数，不重载模块。
4. 仅运行一次 `pm_test=devices`。该测试不是 S3；只有命令返回 `RC=0`、内核出现完整 devices suspend/resume/PM exit 序列、且返回后 `pm_test=none`、`pm_debug_messages=0`、PCI 仍绑定并且 DRM 节点恢复，才判定该对照通过。缺任何一项都不得记为 pass。
5. 等待窗口固定为 **30 秒**：超过 30 秒未返回即记录 `hang`，不继续任何 `pm_test` 级别、不执行 platform 级别、不重复尝试；记录本地控制台最后可见行，按用户在场的恢复决定处理。5 秒 `pm_test_delay` 只用于正常回弹的预期，不是把挂死延长为更大超时的理由。

为避免桌面会话终止导致控制器和日志一起消失，执行器采用上轮已验证的模式：预先由 `systemd-run` 在 `system.slice` 创建独立 root 服务；每一步以独立记录文件写入 `date`、`sync`、RC 和状态快照。会话 ID 从 `loginctl` 输出中按 `ok`/`tty1` 选择并在终止前记录；优先使用会话终止，只有残留 X 客户端时才按既定模板执行 `pkill` 回退。终止前后都记录 `fuser`、PCI driver link、`/sys/class/drm/card*` 和 `/dev/dri/*`，以证明只移除了用户态桌面而没有解绑 GPU。

这项实验的解释规则：

- bound + headless 仍挂死：设备绑定路径是充分的高可信候选，优先进入设备回调代码审查和日志/trace 设计；仍称“候选根因”，除非后续证据能闭合具体函数。
- bound + headless 通过：不能归因设备独立挂死，转入 X/DRM 客户端与设备 suspend 的交互矩阵；此前 unbound 通过不再被当作单变量证据。

### 阶段 C：若 B 通过，最小化的客户端交互对照

只在 B 成功且状态恢复后执行，由 dsh 终审是否需要：

1. bound + headless（基线复核，不重复超过一次）；
2. bound + X server、无桌面客户端；
3. bound + 完整桌面但禁用 picom/portal 等非必要客户端。

每次只改变一个客户端层，使用同一 `pm_test=devices` 入口；不执行真实 `mem`，不在该阶段引入 unbind。若某层挂死，立即停止矩阵，避免重复制造不可恢复现场。

### 阶段 D：代码/运行时收敛

根据 B/C 结果选择最小下一步：

- 若 headless 仍挂死：围绕 PCI PM 回调和 HAL 调用顺序做静态修复候选，优先增加可观测的阶段标记/超时或错误返回处理；任何修复必须单独形成 patch、单测/静态门禁、双构建和新 SHA，不能直接修改已安装 `.ko`。
- 若仅桌面态挂死：围绕 `fantdpu_drm_pm.c` 的 DRM suspend、GEM/CRTC backup 和用户态 DRM master 生命周期做交互调查，不能先改 PCI HAL。
- 若静态无法区分：停止扩大真机风险，保留 `R5=FAIL`，等待可获取的 F HAL 源码、带阶段日志的新构建或 netconsole/第二台机器等更强证据。

### 阶段 E：永久测试补强（调查结论后，不阻塞本轮诊断）

本轮调查不能把一次真机挂死直接变成 CI 可复现用例。后续补强分两层：

1. 单测层扩展现有 suspend 静态门禁，锁定 F lineage 的回调入口、调用顺序、调用方可见的返回值处理和证据字段解析；同时测试 `pm_test` 结果的 `pass|fail|hang|not_run` 状态机、绑定/桌面条件字段和收尾失败时的 fail-closed 行为。夹具只能证明脚本协议，不能模拟 HAL 内部等待。
2. 真机发布门新增一次受监督的 `pm_test=devices` 设备绑定检查和一次真实 `mem` suspend/resume 检查；两者均须使用新 dmesg 窗口、明确的存活依据和恢复快照。没有硬件或没有可用的独立存活通道时，门禁输出 `UNVERIFIED`，不得用构建、fixture 或 `Driver/Firmware OK` 替代。

这解释了原测试未提前发现本问题：原有门禁验证的是补丁应用、构建/载荷、设备节点和计算侧状态，未验证预编译 F HAL 的设备电源回调在目标板上的返回；该缺口应由上述真机门补齐，而非通过放宽 PASS 条件掩盖。

## 3. 存活判定与状态恢复

### 3.1 存活判定

`pm_test` 阶段不得以屏幕是否有信号判定挂死。按以下优先级判断：

1. 命令是否返回并记录 RC；
2. 内核是否出现该阶段的 `suspend complete`、等待和 `resume complete`、`PM: suspend exit`；
3. 若桌面已关闭，以独立 TTY/SSH 是否仍可用作为辅助；
4. 电源键是否触发正常 `poweroff.target` 作为恢复后的生命体征。

headless 黑屏本身是预期现象，不得记录为挂死。反之，命令不返回且本地/独立通道均无响应，才记录 `hang`。

### 3.2 每级结束协议

每一级无论通过还是失败，只要系统返回，都执行并记录：

```bash
sudo sh -c 'echo none > /sys/power/pm_test; echo 0 > /sys/power/pm_debug_messages'
sudo sh -c 'cat /sys/power/pm_test; cat /sys/power/pm_debug_messages'
sudo dmesg --color=never | tail -200
```

预期为 `pm_test=none`、`pm_debug_messages=0`。恢复桌面前确认 GPU 状态、PCI driver link、`/sys/class/drm/card*` 和 `/dev/dri/card*` 正常。

### 3.3 挂死后的恢复

挂死时不得盲目追加测试、切换 `pm_test` 或执行 unbind。用户在本地控制台确认机器无法恢复后，才按既定人工恢复流程重启；重启后第一优先级是确认 `pm_test=none`、`pm_debug_messages=0`、驱动绑定和 DRM 节点，随后才采集日志。若系统能够正常响应，优先使用正常关机，不以“屏幕无信号”作为断电理由。

## 4. 风险与重启控制

1. 不再重复真实 `rtcwake -m mem`：两次失败已经足以判定 R5 阻断，当前目标是定位，不是重复证明失败。
2. `freezer` 已通过，不重复该级；`devices` 失败后不运行 `platform`，避免把已有挂点扩大到更深层。
3. B 阶段不解绑、不重载、不改固件和电源参数；unbind 只在 dsh 另行批准且需要验证设备独立性的情况下使用。
4. 每个挂死实验最多允许一次恢复性重启；重启后先恢复状态并落档，不立即重复同一实验。
5. 所有“命令不返回”实验均要求用户在场；不得使用会在桌面会话终止时一起消失的前台 CLI 作为唯一控制器，必要时由 `systemd-run` 预置独立记录进程，但不把 service timeout 当作能中止内核 D 状态的保证。
6. 不执行 live patch、直接编辑 `/lib/modules`、直接替换已安装模块或手工写入 vendor 树。

## 5. 证据与判定

每个实验单独落盘，文件名包含阶段和条件，例如：

- `r5-bound-headless-pre.txt`
- `r5-bound-headless-run.txt`
- `r5-bound-headless-dmesg.txt`
- `r5-static-review.txt`

文件必须保存 stdout、stderr、RC、开始/结束时间、当前 boot ID、内核、包版本、设备绑定状态、`pm_test` 前后值和 dmesg 采集主体。不得覆盖历史文件；重试必须使用新文件名并在 `install-rollback.txt` 追加索引。

原始特权 dmesg/journal 可能包含主机名、用户路径、设备标识或网络信息，属于本机取证副本，不得未经脱敏直接纳入发布提交。该 P1-1 机制已由 dsh 终裁采纳 **(a)**；(b)/(c) 仅作为被否决的备选记录：

- **推荐 (a)**：原始证据移出仓库归档；Git 只保留脱敏摘要、原始文件大小和 SHA-256、脱敏规则；迁移前先复制并核 hash，禁止删除或改写唯一原件。
- **(b)**：评审并修改 `check-docs.sh` 扫描范围或登记式白名单，明确记录规则变更，不得静默放宽隐私门禁。
- **(c)**：由 dsh 指定其他可审计机制。

迁移已按“复制、双向 hash 校验、归档、写摘要、隐私门禁复核”完成；原始唯一证据保留在上述仓库外归档，禁止改写。进入 Git 的证据必须可追溯且通过隐私审计。

每项结果使用以下字段：

```text
r5_investigation_stage=<static|bound_headless|client_matrix|code_followup>
r5_pm_test_freezer=pass|not_run
r5_pm_test_devices=pass|hang|fail|not_run
r5_pm_test_platform=not_run
r5_gpu_binding=bound|unbound|unknown
r5_desktop_clients=present|stopped|unknown
r5_survival_basis=<command_return|kernel_cycle|independent_tty|poweroff|none>
r5_root_cause=device_suspend_candidate|desktop_drm_interaction_candidate|unresolved
r5_validation_status=FAIL
```

判定边界：

- 任何 `pm_test=devices` hang 继续阻断 R5；不能转成 `n-a` 或 `unverified`。
- bound + headless 通过只排除“无桌面也必挂”的设备独立假设，不使 R5 通过。
- bound + headless 挂死且解绑态通过，可把设备路径列为高可信首要候选，但具体 HAL 函数仍需代码/运行时证据。
- 只有修复后的新包完成真实受监督 `mem` suspend/resume、恢复验证和既定观察项，R5 才可能改为 PASS。

## 6. 放行门与交接

本方案当前仅是调查设计。执行顺序固定为：

1. codex 起草并完成本地静态自检；
2. qoder 初审：核对证据边界、实验变量、安全协议，并提出 P1/P2 修订；
3. dsh 终审：已采纳 P1-1 `(a)`，已批准阶段 A 只读调查；阶段 B/C/D 仍须按阶段明示批准；
4. 仅在对应阶段 dsh 明示批准且用户现场在场后，由用户在本地物理控制台监督执行；
5. codex 负责结果整理和后续实现建议，qoder 初审，dsh 终审提交。

阶段 A 只读审查已获 dsh 放行，`r5-static-review.txt` 仍不能授权或替代阶段 B。阶段 B 及以后必须取得对应的 dsh 明示放行，并由用户现场监督；无论调查结果如何，`R5=FAIL`、`U1/U2`、`validation-results.json`、签发和 tag 均保持冻结。
