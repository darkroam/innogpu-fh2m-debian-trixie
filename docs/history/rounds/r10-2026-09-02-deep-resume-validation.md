# R10 · deep 修复验收（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R10-2026-09-02-suspend-resume彻底修复验收`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R10-2026-09-02-suspend-resume彻底修复验收/narrative.md`，内容 SHA-256：
  `ab592925487e2c3485b90edf70cc7cb5a648fe2cc1cd092f4cca19104b56d1c5`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E3/E6 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R10.md @ 45f6728b1021`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R10/R10-originals.tar @ c247ece00541`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

deep 仍未修复，P3 保持打开；4.0.0-i1 为非 deep 图形基线（每次重启复核
  mem_sleep，不得执行 deep）（report.md#L340-L341、#L392-L393）

## 一、解决什么问题

dotfiles 项目 R09 转交要求：完成挂起/唤醒问题彻底修复——deep 真机验证 patch-024
（≥3 次连续 + 拔电/接电 × 外屏/无外屏组合），通过后提供正式交付版本；patch-025 给出
确定性处置（不得无限 UNVERIFIED）；「deep 长期禁用不可接受，规避不是修复」。

## 二、怎么干的

阶段 0 方案先行：冒烟载体用当前运行的 4.0.1-i3（仅 024），正式候选 4.0.2-i1（epoch
1788537600）；deep 安全网设计为 RTC 90 秒硬件唤醒 + 物理电源键 + 三份回退包（不依赖
userspace watchdog）；受控 deep 触发方法（inhibit logind、机械确认 lid closed、直接写
mem）。dsh 5 项确认批准 + 用户授权 → SSH 判据降级为 NOT_TESTED（无独立 SSH 设备）→
D0 冒烟执行：真实 deep entry/exit，但唤醒失败（用户人工重启），按「任一失败立即
停止」规则 D1-D6、4.0.2-i1 构建与 patch-025 阶段全部取消 → 失败取证、回退 4.0.0-i1
并全量验证。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/report.md#L5-L59 @ 2d96c6ad095b`（阶段 0 版本方案）
- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/report.md#L61-L138 @ 2d96c6ad095b`（deep 安全网与建议矩阵）
- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/report.md#L233-L293 @ 2d96c6ad095b`（D0 结果/硬证据/回退）
- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/report.md#L295-L341 @ 2d96c6ad095b`（patch-024 失败根因与 patch-026 方案）
- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/report.md#L373-L393 @ 2d96c6ad095b`（dsh 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R10-2026-09-02-suspend-resume彻底修复验收/request.md#L14-L59 @ c914d27db76d`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R10/R10-originals.tar @ c247ece00541`（原文快照）
- 事实值（非指针）：本轮无 tracked 变更；HEAD=origin/main=fbb1f003；权威证据目录
  /var/tmp/innogpu-r10/D0-4.0.1-i3-20260902-231546/

## 三、发现了什么问题（错误资产）

1. D0 deep 实质失败（E1）：deep 入口 23:17:49、RTC 唤醒/内核返回 23:19:24，journal 有
   真实 PM: suspend entry (deep)/exit，但用户可见结果为唤醒失败、无法正常处理，最终
   由用户人工重启——该轮不因 kernel suspend exit 而降级为 PASS；控制器结果
   r10_d0_control=FAIL reason=pvr_counters_changed、r10_d0_run=FAIL
   returned_from_suspend=1（report.md#L237-L246）。journal 错误顺序为
   PVRSRVPowerLock() failed (PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF) in
   PVRSRVDevicePreClockSpeedChange() 先发生，随后才出现 InnogpuSysDevPostPowerState()
   state: current=0, new=1；PVR Server Errors 从 0 增长到 1（report.md#L255-L272）。
   处置：矩阵按规则立即停止，4.0.2-i1 未构建，回退 4.0.0-i1 并全验证。教训：deep 的
   判据必须包含用户可见恢复，机械 suspend exit 不能降级 PASS。
2. patch-024 失败根因（E2）：PVRSRVDefaultDomainPower(psDeviceNode) 的封闭对象反汇编
   只执行一次对 psDeviceNode->eCurrentSysPowerState（对象偏移 0x6c）的无锁读取；
   PVRSRVPowerLock() 则先获取 hPowerLock（偏移 0x58），再检查同一字段——这是标准
   TOCTOU 窗口；且当前内核（>= 5.0）RegisterDVFSDevice() 不设置
   pfnDvfsSuspend/pfnDvfsResume，deep resume 期间 devfreq target 得以与 PVR 系统上电
   并发（report.md#L299-L311）。处置：dsh 根因认定「024 不再声称独立修复 deep」，
   批准 patch-026 生命周期同步方向（report.md#L379-L381）。教训：锁外快照不构成同步；
   s2idle 通过不能验证 deep 修复（s2idle 不下电、不触发该窗口）。
3. 回退与验证（E3）：三份 4.0.0-i1 回退包 SHA 均为 68aea6c0…8735；--reinstall
   --allow-downgrades 安装后重启；verify-install-status.sh --require-reboot 4.0.0-i1
   返回 RESULT: PASS_INSTALL_STATUS（report.md#L276-L286）。
   教训：回退须验证安装与运行身份，不能只凭包管理命令返回成功。

4. mem_sleep 不跨重启持久（E4）：回退重启后系统又默认为 s2idle [deep]，已再次显式
   恢复为 [s2idle] deep——sysfs 选择不跨重启持久，在后续正确修复验收前，每次重启都
   必须重新核对，不得执行 deep（report.md#L287-L289）。教训：sysfs 电源策略选择是
   易失状态，必须纳入每轮前置核对。
5. 失败轮 finalize 缺口（E5）：控制器对已启动且失败的轮次没有专用 finalize 动作：
   abort 拒绝已启动轮次，ack 又要求成功标记。在证据固化、回退重启和全部验证完成后，
   本次只删除 49 字节的 active-d0 指针，证据目录未删除；结果为
   r10_failed_state_finalized=PASS 和 r10_evidence_preserved=PASS（report.md#L290-L293）。
   下轮还应为控制器增加「失败取证+回退验证后 finalize」状态及 fixture，不再人工删除
   active 指针（report.md#L351-L352）。教训：状态机必须覆盖失败终态，人工删指针不
   可持续。
6. SSH 判据降级裁决（E6）：用户确认无独立 SSH 设备；dsh 裁决 D0 及后续轮次的网络
   判据降级为 SSH=NOT_TESTED（沿用 R09 先例）；本地网络状态只作旁证记录，不得在交付
   与 P3 回传中声称「外部网络验证通过」（report.md#L224-L228）。
   教训：降低某一取证通道要求须明确裁定，不能顺带降低其他判据。

## 四、怎么解决的

dsh 监督裁决（report.md#L373-L393）：「D0 实质失败成立，处置正确，patch-026 方向
批准。独立核验：journal 错误顺序（PowerLock POWERED_OFF 先于 PostPowerState 0->1）、
PVR Server Errors 0->1、trace 命中 PowerLock 路径而非 cursor 路径、回退后
verify-install-status PASS、mem_sleep 恢复 [s2idle] deep、kprobe/RTC 零残留、门禁全绿
……元结论：s2idle 通过不能验证 deep 修复……教训入档。patch-026 方向批准（生命周期
同步：pvr_pm_suspend 先 SuspendDVFS 等 monitor/在途 target 停止再下电；pvr_pm_resume
先上电完成再 ResumeDVFS；失败回滚语义；<5.0 条件路径避免双调），附执行条件 (a)-(e)
……本轮结论：deep 未修复，P3 保持打开，4.0.0-i1 为非 deep 图形基线（每次重启复核
mem_sleep，不得执行 deep）。」

## 五、特别说明

- 冻结措辞纪律：deep 未修复、P3 保持打开；4.0.1-i3/i4 与未构建的 4.0.2-i1 都不得
  交付；不得以「已通过」覆盖 D0 失败。
- 历史命令不构成执行授权：报告中的 deep 触发、回退与重装命令均为历史记录，不构成
  执行授权；未获得新批准前不再挂起、不构建候选、不更改 tracked 文件。
- 边界纪律：无 tracked 变更；drivers/、baselines/、binary-manifest.json 零改动；
  不做 tag/Release；license/1C 不变。
- 后续影响：patch-026 实现约 0.5-1 个工作日（预计），经 dsh 复审、双构建、安装重启
  与单次 deep 冒烟后才恢复矩阵。
