# R12 · 温度 work 延后（2026-09-03，公开阅读版）

- 记录范围：来源轮次 `R12-2026-09-03-patch-027温度work延后`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R12-2026-09-03-patch-027温度work延后/narrative.md`，内容 SHA-256：
  `647ea50916feee481385f20ce513edd7062a072ac57438e64445d50b652434d7`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E5 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R12.md @ 6402b62535f6`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R12/R12-originals.tar @ b7c2cbf0ce7c`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

4.0.2-i2 整体 deep 验收 FAIL（显示恢复：面板输出/背光/DPU 硬件链路），
  不可交付；patch-025 与本次黑屏无已观测关联，仍 UNVERIFIED（report.md#L242-L245）

## 一、解决什么问题

R11 阶段 2 失败后的新根因：父设备 resume 零延迟重启 hal_temperature_monitor_work，
早于子设备 PVR resume。本轮实现该 work 的延后/门禁修复并升版 4.0.2-i2，完成静态与
离线验证；deep 冒烟另行批准。qoder 本轮起参与（方案建议 + 初审）。

## 二、怎么干的

dsh 三项裁定先行（取代 request 原文冲突条款）：裁定 1 编号用 028（历史
027-foreign-dmabuf-lifecycle.patch 冲突属实，第三次同号冲突）；裁定 2 修复方向采用
codex 精确方案——hal_power_wakeup() 移入 pvr_pm_resume() 且在 PVRSRVDeviceResume()
+ ResumeDVFS() 成功后调用（父设备 resume 路径移除/门禁、boot 路径保留、resume 失败
不启动、suspend 侧 026 语义不动）；裁定 3 阶段 2 改为 3 次连续 deep + 精确 dmesg 判据。
随后实现 patch-028（计数器追加在 struct dev_rsrc 尾部不动闭源字段偏移；S3 挂起后
归零；每个 PVR 子设备 resume 成功后计数、最后一个子设备触发 wakeup；失败/无硬件/
溢出不启动）→ 4.0.2-i2 = 024+026+028、epoch 1788710400 → 阶段 1 提交 0f563dc →
阶段 2 一次 deep：机械判据全过但人工显示判据失败（内屏黑屏）。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/report.md#L5-L39 @ fc43b26af1a9`（patch-028 实现与调用点核实）
- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/report.md#L108-L142 @ fc43b26af1a9`（阶段 2 结果/证据/结论）
- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/report.md#L201-L208 @ fc43b26af1a9`（dsh 裁定记录）
- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/report.md#L213-L255 @ fc43b26af1a9`（dsh 终审×2 与三方共识）
- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/request.md#L55-L91 @ 3d3066b269a4`（dsh 裁定补充）
- 本机来源（非 Git、公开检出不可取得）：`collab/R12-2026-09-03-patch-027温度work延后/qoder-notes.md#L15-L31 @ e01f7e60ac08`（qoder P1-1 编号冲突）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R12/R12-originals.tar @ b7c2cbf0ce7c`（原文快照）
- [028-suspend-resume-hal-temp-monitor-delay.patch](../../../patches/028-suspend-resume-hal-temp-monitor-delay.patch)；历史版本锚：`patches/028-suspend-resume-hal-temp-monitor-delay.patch#L1 @ 0f563dc46182`
- [028-suspend-resume-hal-temp-monitor-delay.md](../../patches/028-suspend-resume-hal-temp-monitor-delay.md)；历史版本锚：`docs/patches/028-suspend-resume-hal-temp-monitor-delay.md#L1 @ 0f563dc46182`
- 事实值（非指针）：i2 deb SHA 9d5d427d…c244f；epoch 1788710400（2026-09-07 00:00
  +0800）；suspend 53/53、finalize 15/15、version 11/11、package 11/11；阶段 1 提交
  0f563dc；阶段 2 无 tracked 变更

## 三、发现了什么问题（错误资产）

1. 阶段 2 人工显示判据失败（E1）：4.0.2-i2 接电/无外屏/合盖一次 deep：机械采集通过
   （PVR 八项计数保持 0；无 3900372、PVRSRVPowerLock failed、POWERED_OFF 或
   PVR_K:(Error)；ftrace 恢复顺序 pvr_pm_resume → PVRSRVDeviceResume → ResumeDVFS →
   hal_power_wakeup → fh2m_inno_queue_dwork 符合 patch-028 设计），但人工显示判据
   失败：开盖后内屏实际黑屏，SSH 可登录；X 根窗口截图含正常桌面内容（X/compositor
   已绘制），DRM eDP-1 connected、link-status Good（report.md#L112-L135）；
   r12_deep_run=PASS 仅表示机械流程成功（RTC 定时、数据采集、脚本执行），不覆盖
   人工显示判据（qoder-notes.md#L273）；/sys/class/backlight、leds、pwm 均无背光/
   PWM 文件，该结果不能单独证明背光关闭，但把故障范围收窄到面板输出/背光或 DPU
   恢复后的硬件输出链路（report.md#L128-L130）。（r12_deep_run 语义与背光 sysfs
   边界并入自 R12-other 版，dsh 裁定核实属实）处置：该轮不执行 ack，不进入后续第
   2/3 轮。三方共识（report.md#L242-L245）：「patch-028
   达成修复目标——R11 的 PVR/温度 work 提前运行根因已消除；但 4.0.2-i2 整体 deep
   验收 FAIL（显示恢复：面板输出/背光/DPU 硬件链路），不可交付。」教训：PVR 层与
   显示层是两个独立恢复面，任一 FAIL 整体即 FAIL；机械判据不能覆盖人工显示判据。
2. 测试计数勘误（E2）：codex report 原声称「全部 17 个 CI/沙箱入口：462/462 PASS」，
   qoder 独立复核见 5 项 pre-existing 偶发失败（exec-probes ×2、VA-API ×1、DMA-BUF
   ×1 首跑/复跑），3 个脚本未受本轮修改；dsh 终审采纳 qoder P2 勘误：「report 原
   "462/462 PASS"表述改为——17 入口共 462 项，本轮 dsh 复跑全数通过；qoder 独立复核
   曾见 5 项 pre-existing 偶发失败……已登记 current-work 待单独排查。计数文档
   （test-strategy 17/462）不受影响」（report.md#L225-L229）；qoder 汇总表记
   457/462、5 项 pre-existing（qoder-notes.md#L196、#L233）。（457/462 并入自
   R12-other 版，dsh 裁定核实属实）教训：全量 PASS 声明必须以独立复跑为准，
   pre-existing 失败须与回归失败分开登记。
3. 停用自动 reboot watchdog（E3）：本次临时控制器未启用自动 reboot watchdog；上一轮
   使用 watchdog 的失败尝试曾因 dmenupass 无法在黑屏场景完成提权而触发自动重启，已
   明确停止使用该机制；为便于 SSH 取证（report.md#L114-L116）。教训：黑屏场景下
   watchdog 的提权路径会反噬取证窗口，取证策略必须随故障形态调整。
4. 编号冲突（E4）：qoder P1-1 指出 request 拟用 patch-027 已被
   docs/patches/patch-027-foreign-dmabuf-lifecycle.md 占用（构建开关
   APPLY_FOREIGN_DMABUF_LIFECYCLE_FIX=1、包版本 3.3.3.42-patched-27）；dsh 裁定改用
   patch-028（qoder-notes.md#L15-L31；report.md#L201-L202）。教训：patches/ 编号
   索引全局唯一，命名前必须查重。
5. 不回退裁决（E5）：dsh 处置裁定「不回退：保持 4.0.2-i2 + mem_sleep=[s2idle] deep
   （i2 的 deep 行为优于 4.0.0-i1：PVR 零错误、SSH 可用、仅显示黑屏且重启可恢复）；
   日常继续 s2idle，deep 仍为实验路径。若用户选择回退 4.0.0-i1 亦可，由用户拍板」；
   用户决策认可并保持 4.0.2-i2（report.md#L246-L249、#L266-L270）。
   教训：不回退是获准的风险选择，不能改写成失败候选已可交付。

6. post-atomic wakeup 光标寄存器（E6）：3 次 post-atomic wakeup 后 cursor_enable=0、
   没有 class=cursor 或 HAL 0x258..0x25a 光标寄存器写入；本次黑屏不能归因于已观测
   的 cursor 寄存器破坏；patch-025 与本次黑屏无已观测关联（report.md#L133-L134、
   #L244）。（并入自 R12-other 版，dsh 裁定核实属实）教训：无观测关联≠排除关联，
   只能如实记录并单独评估。
7. 早期与最终取证口径差异（E7）：早期 qoder 记录「X 根窗口无正常桌面内容」、
   SSH=NOT_TESTED、系统重启后未安装 i2（qoder-notes.md#L262-L263、#L280），与
   report 最终取证（X 已绘制、SSH 可用、终审不回退）不同；保留分歧原文与时间层次，
   最终口径采用 dsh 共识节。（并入自 R12-other 版，dsh 裁定核实属实）教训：审查
   材料中过时状态与最终取证必须保留各自来源和时间层次。

## 四、怎么解决的

dsh 终审（阶段 1，report.md#L213-L230）：阶段 1 通过，批准提交（提交 0f563dc）：
「采纳 qoder 初审（8/8 验收 + 7/7 裁定符合 + 10/10 门禁）并独立复核……门禁全绿：
suspend 53/53、finalize 15/15、version 11/11、package 11/11、license 50/50、collab
26/26、PASS_DOCS、审计 PASS/BLOCKED/CLEARED/BLOCKED。」阶段 2 终审（report.md#L235-
L255）：「共识（三方一致）：patch-028 达成修复目标……但 4.0.2-i2 整体 deep 验收
FAIL……不可交付。……处置裁定：1. 不回退……2. R12 结论：阶段 1 通过（已提交
0f563dc）；阶段 2 = 显示恢复 FAIL。INDEX → 通过。3. 下一步 R13 显示恢复专项（待用户
批准方向）：面板电源/背光/DPU 输出链路在 deep 恢复后的状态与恢复时序……4. P3 回传：
deep 修复分两层——PVR 层已修复（028，真机证实），显示层未修复；P3 保持打开。」

## 五、特别说明

- 冻结措辞纪律：「PVR 层已修复（028，真机证实）」仅限 PVR/温度 work 层，不得外推为
  deep 已修复；4.0.2-i2 整体 FAIL 不可交付；patch-025 仍 UNVERIFIED。
- 历史命令不构成执行授权：报告中的安装、重启、deep 挂起、回退命令均为历史记录，
  不构成执行授权；在方案批准前不再重复 deep。
- 边界纪律：阶段 1 提交 0f563dc；阶段 2 无 tracked 变更；drivers/、baselines/、
  binary-manifest.json、监督分支零改动；license/1C 不变；不做 tag/Release。
- 后续影响：R13 显示恢复专项只读观测优先，不盲目重试 deep；本轮起 qoder 意见保留
  署名、dsh 终审后共识合并定稿（规约 §九，见 report.md#L258-L262）。
