# R5 suspend 阶段 D 设计（030-031 候选）

- 状态：**030-031/i3 唯一受监督复测已执行并再次硬挂；阶段 marker 未持久化，具体挂点仍未闭合**
- 日期：2026-09-15
- 基线：`5.0.0-i2`，O_stage 14 链尾 `c44ce7850134c1455c0935519fc6cb08a81b505804150c59dde1338e1369bd3f`
- 结论边界：`R5=FAIL`；本设计首先提供可观测性和错误传播硬化，不宣称已经修复硬挂

## 1. 输入事实与问题边界

同机矩阵已形成单变量对照：`pm_test=freezer` 通过；桌面态、设备绑定时 `pm_test=devices` 挂死；headless、设备解绑时约 6 秒通过；headless、设备保持绑定时再次硬挂，超过 120 秒且软关机无响应，只能冷断电。阶段 C 因此跳过，设备绑定 suspend 路径是高可信候选，但具体回调或 HAL 函数仍未知。

阶段 B 的持久化内核日志最后一行是 `fantsrvkm/fantdpu_drm_fb.c:220` 的 `fb_helper_is_unbound`。该函数打印后立即返回 `-EINVAL`，所以该行只说明 DRM/fbdev 路径在 suspend 窗口活动，不能证明它自身阻塞。当前 pstore 为空；冷断电会丢失普通 ring buffer，现有日志也没有 `fantgpu_device_suspend()` 各步骤的进入/退出证据。

权威源码审查对象是已安装树 `/usr/src/fantgpu-fh2m-kernel-2.2`，hash 必须等于上述链尾。`build/r16-fantgpu-deb` 是过期中间树，不得用于补丁派生或行号结论。

## 2. 静态调用边界

| 路径 | 实现可见性 | 当前问题 |
| --- | --- | --- |
| `fantdpu_drm_suspend()` → CRTC backup → `fantdpu_drm_hibernation()` → GEM backup | 源码可见 | 回调内部缺少稳定的阶段边界；最后持久日志与此层相关 |
| `ft_pm_suspend()` → `SuspendDVFS()` → `PVRSRVDeviceSuspend()` | 调用方源码可见，部分实现跨 vendor 层 | 030-024/030-026-lifecycle 处理返回与回滚，但没有可持久识别的阶段标记 |
| `fantgpu_pmops_suspend()` → `fantgpu_device_suspend()` | 源码可见 | 无法确认硬挂时是否已经进入 PCI 父设备回调 |
| `fh2m_hal_dma_suspend()` | `hal_dma.c` 源码可见，返回 `int` | 调用方忽略返回值 |
| `hal_power_sleep()` | `fantgpu.o_shipped`，返回 `int` | 内部等待与副作用不可审计；调用方忽略返回值 |
| `hal_check_reg_accessiable()` | `fantgpu.o_shipped`，返回 `void` | 只能做前后标记，不能检查返回值 |
| `disable_irq()` | 内核 API | 可能等待正在执行的 IRQ handler，必须做前后标记 |
| `hal_pdp_restore_default_cfg()` | `fantgpu.o_shipped`，返回 `int` | 内部等待与副作用不可审计；调用方忽略返回值 |
| PCI save/disable/D3 和配置空间 dump | 内核 API + vendor 调用边界 | `pci_save_state()`、`pci_set_power_state()` 返回值未记录；dump 也在函数返回前 |

`fantgpu.o_shipped` 的批准对象 SHA-256 为 `bc2af0729b3eb1b3f25fe7487b4ffa808cb91cbd71ac3a9faa75fb284e4eb1e2`。`hal_power_sleep()`、`hal_check_reg_accessiable()`、`hal_pdp_restore_default_cfg()` 的内部行为只有取得匹配 HAL 源码或新的运行时边界证据后才能判断。

## 3. 030-031 候选范围

### 3.1 第一轮目标

`030-031` 第一轮定义为“suspend 阶段可观测性与返回值硬化候选”，不是未经验证的修复补丁。它只做三类改变：

1. 在 DRM、PVR/DVFS 和 PCI 三层 PM 回调加入稳定的进入/退出 marker，判断硬挂是否发生在 PCI 回调之前。
2. 在 `fantgpu_device_suspend()` 的每个可能阻塞步骤前后加入 marker，成功返回时记录 `rc=0`，非零返回时记录原始 `rc`。
3. 对当前被忽略的返回值建立显式策略；不交换 HAL 顺序，不跳过必要步骤，不使用异步线程包装闭源调用。

禁止在第一轮加入“超时后继续”。内核线程即使在调用者超时后仍可能卡在 HAL 内部，硬件状态未知，继续 suspend 会把诊断问题变成状态破坏。也禁止把 `hal_power_sleep()` 与 `hal_check_reg_accessiable()` 调换：闭源 HAL 可能依赖现有前置条件，当前没有足够证据支持重排。

### 3.2 Marker 契约

marker 直接使用内核 `dev_notice()`，不经过可能属于 vendor 对象的日志封装。固定格式为：

```text
fantgpu_pm_stage=<stage> event=<enter|done|fail|skip> rc=<signed-int> mode=<suspend|freeze> reason=<none|condition_false> propagated=<signed-int|na>
```

`reason` 与 `propagated` 必须始终出现：实际调用的 `enter`/`done`/`fail` 使用 `reason=none`，条件未满足而未执行时使用 `event=skip rc=0 reason=condition_false propagated=na`。`enter` 的 `rc` 固定为 0 且 `propagated=na`；`done` 写实际成功返回值且传播值为 0；`fail` 写原始非零返回值，并且必须写向 PM core 传播的规范化值 `propagated=<signed-int>`。`skip` 只允许用于方案明确列出的条件阶段，不能用来掩盖缺失 marker 或调用失败。不得输出指针、主机名、路径或自由文本。每一对实际调用 marker 必须紧邻目标调用，避免日志顺序与代码顺序脱节。

PCI 层至少覆盖：

```text
pci_callback
device_suspend
dma_suspend
power_sleep
resume_counter_reset
reg_access_check
disable_irq
pdp_restore
pci_save_state
pci_disable_device
get_chip_type
pci_set_d3hot
cfgspace_dump
debug_delay
device_suspend_complete
```

`resize_resume` 位于 `fantgpu_device_resume()` 的条件分支：`resume && has_resize && is_support_resize` 时必须记录实际调用，否则必须记录统一 skip。`debug_delay` 仅在 `acpi_pcie_wait_secs > 0` 时执行，否则输出 `event=skip rc=0 reason=condition_false propagated=na`；`pci_set_d3hot` 仅在 suspend 且平台条件允许时执行，否则使用同一 skip 格式。`suspend=false` 时 `pdp_restore`、PCI save/disable/D3 也统一输出该 skip 事件。实现和测试必须固定这一个通则：条件不满足 = `skip`，调用已执行并成功 = `done`，调用未返回 = 只有 `enter`，调用返回错误 = `fail`；不使用 `done rc=0` 伪装跳过。

DRM/PVR 层至少覆盖：

```text
drm_suspend
drm_crtc_backup
drm_hibernation
drm_atomic_suspend
drm_gem_backup
pvr_suspend
dvfs_suspend
pvr_device_suspend
```

多 CRTC 或多 PVR 子设备不得只靠重复的同名行区分。marker 需要加入非敏感的稳定索引字段，例如 `index=<n>`；禁止记录内核地址。仅添加边界日志，不改变这些回调的现有控制流。

resume 侧必须覆盖 `device_resume_complete` 之前的最小边界，以免一次复测在 suspend 完成后再次挂死时失去归因信息：

```text
device_resume
pci_set_d0
pci_restore_state
pci_enable_device
resize_resume
hal_pci_irq_resume
enable_irq
pvr_resume_counter_inc
pm_resume
hal_power_wakeup
device_resume_complete
```

其中 `pvr_resume_counter_inc` 必须记录 `index=<dev_idx>`、`resumed=<n>`、`dev_nums=<n>`；计数未达到总数、达到总数触发 `hal_power_wakeup`、以及 `resumed > dev_nums` 均须有可解析的结果 marker。`resize_resume` 的实际调用必须记录返回值，条件不满足必须记录 skip。`hal_pci_irq_resume`、`enable_irq` 和 `hal_power_wakeup` 的 marker 必须分别位于对应调用前后，不能只用 `device_resume_complete` 概括。

DRM/PVR 恢复层也必须有对称边界，不能把恢复层挂点归入笼统的“其它设备”：

```text
drm_resume
drm_gem_recover
drm_wakeup
drm_crtc_restore
drm_resume_complete
ft_resume
pvr_device_resume
dvfs_resume
pvr_resume_counter_inc
pvr_power_wakeup
ft_resume_complete
```

`drm_*` marker 对应 `fantdpu_drm_resume()` 的 GEM recover、DRM wakeup、逐 CRTC restore 和返回；`ft_*` marker 对应 `ft_pm_resume()` 的 PVR resume、DVFS resume、030-028 计数聚合和最后一个子设备触发的 `hal_power_wakeup()`。每个实际调用的失败行都必须有 `propagated=`；计数未到总数的合法等待不得写成 fail。多子设备和多 CRTC 使用稳定 `index`，不得记录地址。

### 3.3 返回值与回滚策略

所有列出的 HAL、PCI 和 DRM/PVR `int` 返回值必须被保存并记录；`void` 调用只做前后边界 marker：

- `fh2m_hal_dma_suspend()`：非零时停止向后执行并把规范化负 errno 返回 PM core。实现前必须证明失败路径没有需要调用方补偿的部分 DMA 释放；不能仅因函数返回 `int` 就假定可直接回滚。
- `hal_power_sleep()`：非零时不得继续进入寄存器、IRQ 或 PCI 断电步骤。但 DMA 已经 suspend，而 `fh2m_hal_dma_resume()` 当前实现主体被 `#if 0` 屏蔽，不能把它当成有效回滚。若无法从匹配 HAL 源码证明安全补偿，候选只能记录错误并将该分支标为 `rollback_unproven`，不得声称错误返回后设备仍可继续使用。
- `hal_pdp_restore_default_cfg()`：到达此处时 IRQ 已被禁用且 power sleep 已执行。直接返回会遗留禁用 IRQ，盲目调用 `enable_irq()`、`hal_power_wakeup()` 或 DMA resume 也可能违反闭源 HAL 状态机。未证明完整逆序回滚前，第一轮只记录返回值并保持既有控制流；这是“检测到错误”，不是完整错误传播。
- `pci_save_state()`：非零时尚未调用 `pci_disable_device()`，应记录并终止后续 PCI 断电；但必须先处理前序 IRQ/power 状态的安全回滚，否则该分支仍标为 `rollback_unproven`。
- `pci_set_power_state(PCI_D3hot)`：记录返回值。失败时设备已 disable，不能只返回错误而假装恢复；恢复策略须按 PCI core 约定和本驱动 resume 顺序单独证明。

返回值规范固定为：`ret == 0` 表示成功；`ret < 0`（包括 vendor 常见的 `-1`）原样透传，不做绝对值转换或重新编码；`ret > 0` 才统一规范为 `-EIO`。日志同时记录原始值和传播值。若上述任一回滚无法证明，030-031 的实现评审必须明确选择“只观测、行为保持”或“阻断并有完整回滚”；不得把两者混写。第一轮推荐只改变能够证明安全的错误分支，其余仅 marker + error log。qoder 和 dsh 必须逐分支批准最终选择。

### 3.4 明确不做

- 不改 `fantgpu.o_shipped`，不二进制 patch，不伪造 HAL 完成。
- 不新增可在运行时跳过任一 HAL/IRQ/PCI 步骤的模块参数。
- 不调换现有 `dma_suspend → power_sleep → reg_access → disable_irq → pdp_restore → PCI` 顺序。
- 不把 watchdog、workqueue 或 kthread timeout 描述成能取消卡住的同步 HAL 调用。
- 不以 marker 全部打印、`pm_test=devices` 返回或构建通过替代真实 `mem` suspend/resume 的 R5 判定。

## 4. 实现与生成链

实现获批后必须走双轨，不允许 builder post-trace 打补丁：

1. 从 14 链批准尾树派生 `patches/030-031.patch`，并创建 `patches/030-031.meta.json`；meta 分别记录 semantics、license、dependencies、O_stage adaptation、补丁 SHA、before/after tree hash 和 `runtime=pending`。
2. `materialize-o-stage.sh` 链从 14 增至 15，`run-030-meta-tests.sh`、materialize tests 和 builder gates 同步锁定新链尾；fresh replay 必须 `--fuzz=0` 且无 `.orig/.rej`。
3. 因源树已变化且现有 `5.0.0-i2.meta.json` 声明不可变，不得覆盖 i2 失败代。版本已裁定为新候选 `5.0.0-i3`，生成同代五件产物和 meta，并把 i2 保留为已安装失败证据；不得通过书面例外或静默覆盖复用 i2。
4. builder 只消费新的锁定快照，更新链数、树 hash、meta 名和 Description；release package、payload integrity、md5sums、Installed-Size 与全部 F lineage 门禁重跑。
5. 在同一环境双构建，要求候选 deb 字节一致并产生新的 SHA-256。旧 `86e7f12a…` 继续作为 R5 失败包，不得改写其证据含义。
6. 新增设计、patch/meta、测试和生成产物进入 index 后，按既定两阶段流程重新生成 project-tools allowlist，并复跑许可证审计；不得在文件仍 untracked 时预写 allowlist 条目来制造 `allowlist_untracked`。

030-031 实现前至少增加静态测试，锁定 marker 的唯一格式和严格顺序、每个 `int` 返回值确实被接收、禁止 HAL 重排、禁止异步 timeout/skip 参数，并证明 patch 已随 15 链进入快照。fixture 只能验证源码控制流和解析器，不能模拟闭源 HAL 已返回。

## 5. 唯一复测门

本节原为复测放行门；dsh 后续已明示放行，唯一一次实验于 2026-09-15 执行。以下条件保留为审计依据：

1. 新包 SHA、DKMS 模块对象 SHA、build ID 和 15 链 provenance 均核对一致，runtime health gate 通过。
2. 已有经实测可在目标 suspend 区间保存 marker 的带外通道。首选串口/独立硬件控制台；netconsole 只有在接收端、网卡 suspend 顺序和丢包边界完成演练后才可采用。当前 pstore 为空，不能作为已满足条件。
3. 启动参数必须显式包含并在 `/proc/cmdline` 核验 `no_console_suspend ignore_loglevel`；这两个参数用于降低 console suspend 和 printk level 过滤造成的观测缺口。测试前仍须向带外接收端发送并核对本构建的 marker 自检行；若平台不能保证该参数语义，则改用已实测的 `dev_warn` marker 方案并由 dsh 重新批准，不能默认认为 `dev_notice()` 可见。
4. 用户在本地监督，预先准备批准的回退包及 SHA；不依赖 SSH 或 GPU 屏幕作为唯一存活/取证通道。
5. 仍只执行一次 bound+headless `pm_test=devices` 定位运行。30 秒不返回即停止追加操作；不得紧接着做真实 `mem`、第二次 pm_test 或现场试验性 unbind。

复测输出按 marker 最后完整 `done` 与下一个 `enter` 定位候选区间：有 `enter` 无 `done/fail` 只证明阻塞发生在该调用边界内或其日志尚未可靠送达，必须同时满足带外通道连续性才能归因。决策树固定如下：

1. DRM/PVR marker 未闭合：候选在对应 DRM/PVR 子设备回调；不得跳到 PCI/HAL。
2. DRM/PVR 完整但 `pci_callback`/`device_suspend` 未进入：候选在 PM core 的后续设备排序或未覆盖的子设备回调。
3. PCI suspend marker 在某个 HAL/IRQ/PCI 调用处只有 `enter`：该调用是高可信候选；仍需带外链路确认 marker 没有丢失。
4. **全部 suspend marker（包括 `device_suspend_complete`）均已完成，但仍未进入 resume 或 `pm_test` 仍挂死**：判定 GPU suspend 回调已返回，候选转为 PM core 后续设备/平台阶段或测试控制器，不得继续归因任何 HAL。
5. PCI resume marker 已进入但 `resize_resume`、`hal_pci_irq_resume`、`enable_irq` 或 `device_resume_complete` 未闭合：候选留在 PCI resume 路径；若 `resize_resume` 为条件不满足，必须以 `skip` 而非缺行解释。
6. PCI resume 已闭合但 `drm_resume`/`drm_resume_complete` 或 `ft_resume`/`ft_resume_complete` 未闭合：候选分别转为 DRM 恢复层或 FT/PVR 恢复层，不得归为“其它设备”。
7. suspend 与 PCI、DRM、FT/PVR resume marker 均完整到各自 complete，但命令仍挂死：候选转为 PM test 收尾、其它设备或观测控制器；本实验不再追加测试。

`skip` 事件不参与“调用已完成”判断；只有实际调用的 `done` 或明确的阶段 `*_complete`（`device_suspend_complete` / `device_resume_complete` / `drm_resume_complete` / `ft_resume_complete`）才能闭合对应阶段。

复测正常返回仍只表示 `pm_test=devices` 定位门通过。之后是否允许一次真实 `mem` 由 dsh 重新裁决；只有真实 suspend/resume、驱动/固件、PCI/DRM、桌面、渲染与内核 fault 恢复门全部通过，`R5` 才可从 FAIL 改为 PASS。

## 6. 判定与停批

- 构建或静态门禁失败：不安装，返工。
- 无可靠带外 marker 通道：不复测，保持 `R5=FAIL`，等待 HAL 源码或取证能力。
- 新包 runtime health 失败：不运行 pm_test，回退。
- 唯一 pm_test 再次硬挂：冷断电后只做恢复和证据闭合，不重复；按最后闭合 marker 缩小候选，但 R5 继续 FAIL。
- HAL 返回非零且回滚未证明：立即停批，不把错误传播本身写成修复成功。
- pm_test 通过但真实 `mem` 未获批或未通过：R5 继续 FAIL。

始终冻结 `U1/U2`、`validation-results.json`、签发和 tag。030-031 的 `runtime` 状态在真实恢复验证前保持 `pending`。

## 7. 初审重点

qoder 初审按 findings 优先，至少回答：

1. 三层 marker 是否能区分 DRM/PVR 子设备回调、PCI 回调未进入和 PCI/HAL 内部挂点；是否仍有返回前未覆盖的阻塞调用。
2. `dev_notice()` 与字段格式是否适合带外采集，索引字段是否避免多设备歧义且不泄露敏感信息。
3. 每个错误分支的“行为保持/错误传播/回滚”是否写清，尤其 `disable_irq()` 后和 PCI disable 后是否存在伪安全返回。
4. 设计是否错误承诺 timeout 能取消闭源同步调用，或未经证据改变 `hal_power_sleep()`/`hal_check_reg_accessiable()` 顺序。
5. 带外日志是否确实作为复测硬前置；netconsole、pstore 或屏幕是否被误当作天然可靠。
6. 030-031 双轨、15 链、不可变 meta、新候选版本、builder、双构建和新 SHA 的传导范围是否完整。
7. PASS 边界是否保持为真实 `mem` + 全恢复门，而非 marker 或 pm_test 单项通过。

qoder 只做只读初审，不创建补丁、不构建/安装、不执行 pm_test。输出 `通过`、`修订后通过` 或 `返工`；P1/P2/P3 必须附文件/行号与理由。

## 8. 实现批结果（2026-09-15，待 qoder 初审）

- `030-031` 只改 `fantgpu_pci_drv.c`、`fantdpu_drm_pm.c`、`ft_drm.c`；patch SHA `a06b6993…`，before `c44ce785…`，after `acfe80d1…`。
- PCI 层的 `fh2m_hal_dma_suspend()`、`hal_power_sleep()` 失败会停止后续 suspend 并按“负值原样、正值 -EIO”返回。前者的已知失败入口在副作用前；后者仍明确标记 `rollback_unproven`，不得把错误返回描述为设备已恢复可用。
- `hal_pdp_restore_default_cfg()`、`pci_save_state()`、`pci_set_power_state(PCI_D3hot)` 在原实现中返回值被忽略；本轮保存并输出 fail marker，但因 IRQ/power/PCI 半状态缺少已证明回滚，保持原有继续执行控制流，fail 行写 `propagated=0`。这三支是“检测但不传播”，不是伪造成功。
- resume 侧原实现忽略 `pci_set_power_state(PCI_D0)` 的返回值，且 `pci_enable_device()`、`fantgpu_resize_resume()` 失败后只记录错误并继续。本候选保存这三项返回值并在失败时停止后续 resume，连同 DRM/PVR/DVFS 的既有错误返回统一按负值原样、正值 `-EIO` 传播；void HAL/IRQ 调用仅做紧邻边界 marker。该 fail-fast 行为是本轮有意的返回值硬化，仍须由初审逐分支确认。
- i3 五件隔离在 `docs/planning/evidence/o-stage/5.0.0-i3/`，未覆盖 i2 失败锚点；双构建 deb SHA `f2e821f2…`，562 行 md5sums 字节一致。该结果不改变 `R5=FAIL`、runtime pending 或复测硬前置。

## 9. 唯一复测结果（2026-09-15）

候选 deb SHA、15 链树 hash、DKMS 模块、vermagic、Driver/Firmware、DRM 节点和 runtime health 均在实验前通过。启动命令行包含 `no_console_suspend ignore_loglevel`，`console_suspend=N`；停止 `startx/dwm` 后保持 `fantgpu` 与 PCI 设备绑定，控制台自检行可见。

单次 `pm_test=devices` 触发后命令未返回、SSH 不可达，最终由操作者冷启动恢复，结果为 `HANG`。恢复后 `pm_test=none`、`pm_debug_messages=0`、Driver/Firmware OK、PCI/DRM 节点恢复；不允许重复实验。原始证据保存在 ignored 目录 `.runtime-archive/runtime-5.0.0-i3/`，Git 摘要见 `docs/planning/evidence/o-stage/runtime-5.0.0-i3/`。

阶段归因未闭合：持久日志只有 `control_selftest`、`control_trigger` 两条控制 marker，驱动 marker 为 0 条；pstore 为空，最后持久内核事件为 VPU prepare-suspend。操作者未能在快速滚动后拍下控制台最后行。用户态日志进程在 device suspend 前已被冻结，GPU 控制台又不是独立于被测 DRM 设备的通道，因此本次“控制台自检可见”不足以满足 §5.2 的可靠带外保存要求。应将本轮观测门记为 `FAIL`、`stage_attribution=UNVERIFIED`，不得套用 7 条决策树猜测 HAL 挂点。

后续任何定位运行必须重新裁决，并先演练真正独立且可保存目标区间 marker 的串口或 netconsole 接收端。不得再以本机 GPU 屏幕、journald、`dmesg -w` 或空 pstore 作为单一持久证据通道。`R5=FAIL`、U1/U2、validation-results、签发和 tag 继续冻结。
