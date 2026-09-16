# R5 suspend 030-032 诊断设计

- 日期：2026-09-16
- 状态：**已按 dsh 三线终裁修订，待 qoder spot 与 dsh 提交终审；未实现、未构建、未执行真机探针**
- 候选：`030-032`，仅用于 `5.0.0-i4` 诊断包，不是发布修复
- 基线：`5.0.0-i3` / 15 链树 `acfe80d1cff9437f8d4a77ee71640d1a4c0698614656f8312cdf662d8366361c`
- 冻结：`R5=FAIL`，U1/U2、validation-results、签发和 tag 不变

## 1. 问题与新证据

i3 的唯一 bound+headless `pm_test=devices` 再次硬挂。驱动 marker 没有持久化，故 `observability_gate=FAIL`、`stage_attribution=UNVERIFIED`，不得依据最后一条 VPU 日志推断挂点。dsh 已禁止重复同类实验，后续只能执行重新审批的 i4 诊断序列。

EFI pstore 已注册，但当前内核未启用 `CONFIG_PSTORE_CONSOLE`。普通 `dev_notice()` marker 不会写入 EFI pstore，且冷启动后 pstore 为空；运行时参数无法补上未编译的 frontend，因此本轮不改 EFI 变量或启动配置。能力记录见 `runtime-5.0.0-i3/r5-pstore-efi-check.txt`。

Ghidra 与 objdump 对 O/F 预编译对象的三函数比较表明：

| 函数 | O/F 结果 | 可见行为 |
| --- | --- | --- |
| `hal_power_sleep` | 都是 63 字节，控制流同构 | 同步取消 monitor delayed-work，再 flush monitor workqueue |
| `hal_check_reg_accessiable` | 语义同构；F 多栈保护 | 单次寄存器读取，无轮询 |
| `hal_pdp_restore_default_cfg` | 都是 74 字节，控制流同构 | 查找资源后调用 chip callback |

没有发现 F 在三个 wrapper 中新增无超时轮询或固件握手。最具体的新候选是 `hal_power_sleep()` 等待 monitor worker/workqueue 排空，但仍只是待运行时验证的候选。完整行为摘要见 `runtime-5.0.0-i3/r5-hal-o-vs-f-reverse-notes.txt`。

## 2. 设计裁剪

原提案要求从 debugfs 任意单调四个 HAL 函数，并假设成功后都能安全反卷。静态证据不支持该假设：

1. 当前构建未定义 `ENABLE_DMA_INTERNAL_MANAGER_CHAN`；但 i3 已安装模块中的 `fh2m_hal_dma_suspend()` 会调用 `hal_dma_chan_release_callback()`，由其遍历 DMA channel pool 并释放空闲 channel，随后返回 0，不能描述为纯空转。该调用会改变 DMA channel 状态，不适合作为无恢复动作的单调探针。
2. `hal_pdp_restore_default_cfg()` 在原序列中位于 power sleep、寄存器检查和 `disable_irq()` 之后；脱离前置状态调用会产生不可解释结果。
3. `fh2m_hal_dma_resume()` 主体被 `#if 0` 屏蔽，不能承诺 DMA suspend 后可恢复。
4. `hal_pdp_restore_default_cfg()`、PCI disable/D3 后的完整逆序恢复尚未证明，不能提供通用 stop-stage 反卷。

因此 i4 第一轮只实现能够回答当前最强假设、且有明确恢复动作的最小接口。其余调用保持不暴露；若 qoder/dsh 要求扩大范围，必须先逐项证明前置状态和恢复序列。

## 3. i4 debugfs 主探针

### 3.1 接口

在 `fantgpu_pci_drv.c` 复用现有每卡 debugfs 目录，新增 root-only `pm_probe` 文件。只接受两个固定命令，不接受函数名、地址、阶段编号或任意参数：

```text
run reg-read confirm=R5_I4_DIAGNOSTIC
run monitor-cycle confirm=R5_I4_DIAGNOSTIC
```

写入端必须满足 `CAP_SYS_ADMIN`、设备绑定、`SYSTEM_RUNNING`、驱动内 `pm_transition=false`、`removing=false`、单执行者和精确命令匹配，否则失败关闭。状态只允许 `idle -> running -> returned|failed`；运行中和完成后拒绝同 boot 重入。debugfs 节点的 read 返回固定字段：probe、state、raw_rc、normalized_rc、entered_step、completed_step。不得返回指针或自由文本。

探针状态、`pm_transition` 和 `removing` 由每卡私有 mutex 保护。debugfs write 从最终条件复核到 HAL 调用及结果落状态全程持锁；PCI suspend/resume/freeze/thaw/poweroff/restore 回调也在同一把锁内设置/清除 `pm_transition` 并完成实际 PM 调用，使已进入本驱动回调的 PM 与探针串行。shutdown 路径使用同样的互斥规则。

生命周期按“先关入口、后等在飞调用、最后释放资源”处理：`pm_probe` fops 设置 `.owner = THIS_MODULE`，read/write 使用 `debugfs_file_get()`/`debugfs_file_put()`；remove 先原子置 `removing=true` 并移除 debugfs 节点，再取得同一 mutex。若探针正在执行，remove/unload 只能等待其返回；任何不能等待的管理入口必须 fail-closed 拒绝，不能越过锁释放 `pdev_rsrc`。取得锁后再次确认无 `running` 状态，才允许继续 `hal_power_deinit()` 和 `dev_rsrc_deinit()`。实现测试必须覆盖“remove 先到”“write 先到”和已打开 fd 在节点移除后写入三种顺序。

该互斥只保证进入 fantgpu PCI PM 回调后的本地串行，不是全局 system-suspend 锁。系统 PM 可能已启动但尚未调用本驱动回调，形成“write 最终检查通过后、PM core 到达本设备前”的残余窗口。因此保证级别明确为 best-effort：真机协议同时要求 `pm_test=[none]`、无 `systemctl suspend`/`rtcwake`/hibernate 发起者，并在独立服务启动前后各复核；不得把 `SYSTEM_RUNNING` 或私有标志描述成全局强保证。

该接口只存在于 030-032/i4 诊断构建。任何发布候选必须完全反向移除 030-032；门禁不只检查源码，还须检查物化 snapshot/manifest、builder 输入树、DKMS 安装树、最终 `.ko` 的 symbol/string 以及 deb 文件清单/解包载荷，证明 `pm_probe`、确认 token、诊断状态字段和对应 debugfs fops 均不存在。仅把权限改小、隐藏节点或从补丁清单删名不算移除。

### 3.2 `reg-read`

直接调用 `hal_check_reg_accessiable()`。调用前后各写一个 `dev_warn()` marker。它只验证单次寄存器读取是否返回；返回不代表后续 suspend 序列安全。

### 3.3 `monitor-cycle`

按原对象语义调用 `hal_power_sleep()`，返回后立即调用 `hal_power_wakeup()` 恢复 monitor delayed-work。marker 固定覆盖 `power_sleep enter/done` 与 `power_wakeup enter/done`。`hal_power_sleep()` 声明为 `int`，负值原样、正值转 `-EIO`；`hal_power_wakeup()` 的 O/F 反汇编均为空指针检查后以 delay 0 调用各自 queue-dwork wrapper 并恒返 0（F/O 均 47B），仍按实际返回值记录。对当前批准对象，这证明 sleep 正常返回后的 wakeup 足以重新排队 monitor 循环，因此成功轮次无需仅为恢复该 work 而重启；健康门仍必须通过。

若写调用超过 30 秒未返回，只能判 `monitor-cycle=HANG`；timeout 不能取消内核同步调用。此时不得再访问 GPU debugfs、卸载模块或继续其它探针。若系统仍可通过独立 SSH/TTY 操作，只允许采集 blocked task、SysRq task dump 和 dmesg，然后由操作者决定软关机；不可达时按既定恢复协议处理。**锁内挂死轮次的预期恢复方式为硬断电：软关机/reboot 可能因探针持锁与 shutdown 互斥而不完成（qoder 初审 P3-1 记录项；实现批若给出 shutdown 取锁顺序的容忍论证可改写本行）**。

### 3.4 更细边界

若 `monitor-cycle` 挂死，下一版不再单调其它 HAL，而是在源码可见的 `fh2m_fant_cancel_dwork_sync()` 与 `fh2m_fant_flush_workqueue()` 调用边界增加 marker，并优先用 ftrace/kprobe 获取 monitor worker 栈。只有该补强仍无法区分 cancel 与 flush 时，才考虑第二个拆分探针；不得在同一 i4 轮次临时扩展。

## 4. 条件式 stop-stage 备选

只有两个主探针都返回且 runtime health 恢复后，才允许 qoder/dsh 另行批准 stop-stage 构建。它不作为 i4 首轮默认测试。

stop-stage 必须复用 `fantgpu_device_suspend()` 的真实顺序，并在检查点返回一个明确错误，使 PM core 自动退出测试。只允许在已证明可恢复的边界启用：进入 PCI 回调前、`hal_power_sleep()` 前，以及 power sleep 返回后调用 `hal_power_wakeup()` 能闭合的边界。`disable_irq()` 后最多允许在 `enable_irq()` 加 power wakeup 均已静态证明时使用。

每个检查点的评审表必须逐项记录已执行阶段和逆序动作，并额外核验：未成功执行 `pci_save_state()` 时禁止调用 `pci_restore_state()`；只有 `disable_irq()` 已完成时才能配对 `enable_irq()`，且只配对一次；`fantgpu_resize_resume()` 只能在真实 resume 已完成 PCI D0、restore、enable 及其原有 chip capability 前置时调用，不能拿来充当 stop-stage 的通用回滚。任一前置无法证明即删除该检查点，而不是标为“尽力恢复”。

禁止在 `hal_pdp_restore_default_cfg()`、PCI save/disable、D3 或 cfgspace dump 之后设置 stop-stage。不得声称 systemd timeout、kthread 或 workqueue 能取消卡住的同步调用。每增加一个检查点，必须有一条静态测试锁定原调用顺序、退出 errno 和逆序恢复；没有恢复证明的检查点不实现。

## 5. 真机执行协议

真机操作继续由用户在场监督，且实现批经过 qoder 初审和 dsh 终审后才可生成脚本。每个探针使用独立新启动状态，最多执行一次：

1. 核验 i4 deb SHA、安装树 hash、模块 SHA/vermagic、Driver/Firmware、PCI 绑定和 DRM 节点。
2. 停止 startx/dwm，记录 fuser 和节点状态；主探针不进入 suspend，`pm_test` 必须保持 `none`，并确认没有 `systemctl suspend`、`rtcwake` 或 hibernate 发起者。
3. 先在持久文件写入 boot ID、probe、开始时间和 `armed=1`，执行 `sync`，再由独立 `systemd-run` 服务写 debugfs。
4. 返回时写 RC、debugfs 状态、dmesg、健康检查和 `completed=1`，再次 `sync`。30 秒无返回写不出完成字段是预期，不以缺行冒充成功。
5. 重启后收集器按同一 boot ID 判定：`armed=1` 且无 `completed=1` = `HANG_OR_FORCED_RECOVERY`，并记录操作者恢复方式；脚本自动生成 SHA 摘要，操作者不手抄 marker。

存活判定依次使用命令返回、独立 SSH/TTY、SysRq task dump 和电源键软关机响应。GPU 屏幕只作辅助，黑屏不等于挂死。任何探针结束或恢复后都确认 `pm_test=none`、`pm_debug_messages=0`、PCI/DRM/Driver/Firmware 状态；异常即停止后续轮次。

## 6. 判定树

| 结果 | 结论 | 下一步 |
| --- | --- | --- |
| `reg-read` 挂死 | 单次寄存器读取边界为高可信候选 | 停批，采集任务栈；不运行 monitor-cycle |
| `reg-read` 返回、`monitor-cycle` 挂死 | monitor work cancel/flush 边界为高可信候选 | 停批，补 worker 边界 marker/ftrace |
| 两者返回且健康恢复 | 三个 wrapper 的独立正常态调用未复现 | 只说明存在 PM 状态或设备排序依赖；评审 stop-stage 备选 |
| 任一返回错误或健康未恢复 | 诊断导致错误或残余状态 | 停批并回退，不解释为原挂点 |
| 30 秒无返回但机器仍活 | 调用线程阻塞 | 只读采集任务栈后恢复，不发送第二次命令 |
| 机器硬挂 | i4 仍失败 | 最多一次恢复性重启；本批不追加实验 |

任何 debugfs 探针 PASS、stop-stage 返回或 pm_test 定位成功都不能把 R5 提升为 PASS。R5 仍要求真实 suspend/resume 与完整恢复门通过。

## 7. 实现、测试与证据

实现批至少包含 `patches/030-032.patch`、四目 meta、i4 隔离物化产物、诊断构建 SHA 和双构建一致性。i3 失败锚点不得覆盖。静态测试至少锁定：root/capability 门、精确命令语法、单 boot 不重入、PM/remove/probe 同锁顺序、debugfs 文件引用、返回值规范、monitor sleep/wakeup 顺序、禁止暴露 dma/pdp 命令、禁止异步 timeout，以及发布源码、物化树、DKMS 树、模块和 deb 载荷必须移除全部诊断入口/符号/token。

原始真机输出进入 ignored `.runtime-archive/runtime-5.0.0-i4/`；Git 只保留每文件 SHA-256/字节数/脱敏规则/归档路径及机器可解析结论。不得记录主机名、用户名、绝对 home 路径或内核地址。门禁为 `check-docs`、license audit、协作校验、R16 gate 与相关单测全绿。

## 8. 初审重点

qoder 只读初审应优先确认：

1. 否决任意四函数单调及通用安全反卷是否与实际副作用一致。
2. `hal_power_wakeup()` 的 47B 反汇编证据是否足以支持“成功轮次无需仅为 monitor work 恢复而重启”的限定结论。
3. debugfs 并发、生命周期、remove 与 PM transition 互斥是否闭合。
4. 30 秒 timeout 是否只用于判定而未被描述成能取消内核调用。
5. i4 诊断代码从发布候选完全移除的门禁是否可执行。
6. O/F 反编译结论是否避免把同构 wrapper 误写成已证明根因。

初审输出 `通过`、`修订后通过` 或 `返工`，P1/P2/P3 均附文件行号。qoder 不修改工作树、不构建、不安装、不触发 debugfs 或 PM。
