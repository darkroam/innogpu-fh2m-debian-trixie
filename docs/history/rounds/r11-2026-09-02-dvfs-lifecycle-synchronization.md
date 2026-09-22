# R11 · patch-026 生命周期同步（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R11-2026-09-02-patch-026生命周期同步修复`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R11-2026-09-02-patch-026生命周期同步修复/narrative.md`，内容 SHA-256：
  `bb707b1747d8ce8ef3aa51b5537f21f43b2bf7dc159bbbe3fdc5bc4c50e15df6`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E2/E4/E7 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R11.md @ 6e35cd5d8da5`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R11/R11-originals.tar @ 4117818ba087`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

deep 仍未修复，P3 保持打开，4.0.0-i1 为非 deep 基线（report.md#L238-L239）

## 一、解决什么问题

D0 已失败（根因 TOCTOU），dotfiles 转交要求验证失败则继续修复并给出下一步计划与
预期时间。本轮实现 patch-026（devfreq/PVR 电源生命周期同步）并完成全部静态与离线
验证；真机 deep 冒烟仅在全绿 + dsh 审查 + 用户确认后作为阶段 2 单独执行。

## 二、怎么干的

同步语义核实（从 Debian linux-source-6.12 6.12.101-1 精确核实：devfreq_monitor() 持
devfreq->lock 执行 target，devfreq_monitor_suspend() 取同锁 + stop_polling +
cancel_delayed_work_sync → SuspendDVFS() 真正等待在途 target 并排空后续轮询，无需另造
锁）→ 实现 patch-026（只改 innosrvkm/pvr_drm.c：suspend 先 drain DVFS 再 PVR 下电，
失败恢复 DVFS；resume 先 PVR 上电成功后才 ResumeDVFS；<5.0 条件避免双调；patch-024
保留为防御性快速路径并明确其 TOCTOU）→ 新候选 4.0.2-i1 = 024+026、epoch 1788624000
→ 新增失败 finalize 工具（dirfd/openat/O_NOFOLLOW + 12 项隔离 fixture）→ 阶段 1 全
门禁 → 用户批准后阶段 2 deep 冒烟失败并回退。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R11-2026-09-02-patch-026生命周期同步修复/report.md#L7-L39 @ 6c39336f6b1c`（同步语义核实/patch-026/finalize 工具）
- 本机来源（非 Git、公开检出不可取得）：`collab/R11-2026-09-02-patch-026生命周期同步修复/report.md#L126-L142 @ 6c39336f6b1c`（dsh 阶段 1 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R11-2026-09-02-patch-026生命周期同步修复/report.md#L156-L217 @ 6c39336f6b1c`（阶段 2 冒烟/新根因/恢复）
- 本机来源（非 Git、公开检出不可取得）：`collab/R11-2026-09-02-patch-026生命周期同步修复/report.md#L222-L239 @ 6c39336f6b1c`（dsh 阶段 2 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R11-2026-09-02-patch-026生命周期同步修复/request.md#L8-L48 @ 6906f6ca9692`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R11/R11-originals.tar @ 4117818ba087`（原文快照）
- [026-suspend-resume-dvfs-lifecycle.patch](../../../patches/026-suspend-resume-dvfs-lifecycle.patch)；历史版本锚：`patches/026-suspend-resume-dvfs-lifecycle.patch#L1 @ 6e3c8ce033b0`
- [026-suspend-resume-dvfs-lifecycle.md](../../patches/026-suspend-resume-dvfs-lifecycle.md)；历史版本锚：`docs/patches/026-suspend-resume-dvfs-lifecycle.md#L1 @ 6e3c8ce033b0`
- [finalize-suspend-resume-failure.py](../../../tools/finalize-suspend-resume-failure.py)；历史版本锚：`tools/finalize-suspend-resume-failure.py#L1 @ 6e3c8ce033b0`
- 事实值（非指针）：4.0.2-i1 deb SHA e115bdcd…0c09e；epoch 1788624000（2026-09-06
  00:00 +0800）；suspend 39/39、finalize 12/12、version 10/10、package 11/11、
  17 入口 444/444

## 三、发现了什么问题（错误资产）

1. 阶段 2 deep 冒烟失败（E1）：2026-09-03 01:01:05 deep 入口、RTC 90 秒内核
   01:02:39 返回，journal 有真实 entry/exit；机械判据失败：Server Errors 从 0 增至 1，
   journal 再次出现 PVRSRVPowerLock() failed (PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF)
   in PVRSRVDevicePreClockSpeedChange()，且发生在 InnogpuSysDevPostPowerState()
   state: current=0, new=1 之前；用户可见判据失败并人工重启；按任一失败立即停止规则
  未执行第二次 deep、连续组或供电/外屏矩阵（report.md#L175-L185）；SSH 判据按批准
  协议为 NOT_TESTED（report.md#L150、#L184）。（SSH=NOT_TESTED 并入自 R11-other
  版，dsh 裁定核实属实）教训：patch-026 只关闭了 devfreq monitor 的并发源，deep
  失败说明还有其他并发入口。
2. 新根因证据（E2）：本次错误后一行来自 hal_temperature_monitor_work:2152 的
   recover gpu pll is: 1350，与 R10 D0 的相同错误序列一致。封闭 HAL 反汇编证明该 work
   在恢复 PLL 前后调用 fh2m_hal_gpudrv_clkchange(dev, PRE/POST)，PRE 路径进入 PVR 的
   PVRSRVDevicePreClockSpeedChange()；hal_power_sleep() 会 cancel_dwork_sync 并 flush
   温度监控 workqueue，但 hal_power_wakeup() 以 delay 0 重新排队该 work；PCI 父设备
   resume 恢复硬件后立即调用 hal_power_wakeup()，PVR 是其子 platform device、子设备
   pvr_pm_resume() 随后才执行——父设备把温度 work 零延迟启动后，它仍可在子设备
   PVRSRVDeviceResume() 完成前调用 PVR 时钟变更回调（report.md#L189-L203）。教训：
   一个时钟变更入口可能由多个并发源共享，修复必须穷举所有源。
   处置：后续候选改为 PVR 子设备恢复成功后启动温度 work，阶段 2 仍判失败；该方向的批准不追认为本轮已修复。

3. finalize round ID 缺陷（E3）：从 R10 控制器复制出的私有控制脚本把 active pointer
   写成绝对证据路径，并使用大写 S0；阶段 1 新增的 fail-closed finalize 工具只接受小写
   安全 round ID——两次 finalize 均以 invalid_active_pointer_content 正确拒绝。随后把
   证据目录与三个绑定标记规范化为小写 s0-4.0.2-i1-20260903-005804，再运行同一工具
   成功：只删除 active pointer，保留全部证据和 failure-finalized（report.md#L212-L217）。
   教训：控制器必须直接生成合法 round ID，并新增「绝对路径/大写 ID 拒绝与正常
   finalize」端到端 fixture（dsh 已批准该补齐）。
4. 回退与验证（E4）：三份回退包哈希再次一致后，以 --reinstall --allow-downgrades
   安装 4.0.0-i1 并再次重启；verify-install-status.sh --require-reboot 4.0.0-i1 为
   PASS，PVR 八项归零，硬件 GL/DRI3 与设备节点正常，当前保持 [s2idle] deep
   （report.md#L207-L211）。
   教训：回退验收与失败候选的修复结论分开，恢复可用不等于修复成功。

5. 编译警告诚实边界（E5）：离线 DKMS 构建日志共报告 4422 条编译警告，很可能主要
   来自既有厂商源码，但本轮未与旧候选做逐条警告基线对比，因此只能确认构建成功，
   不能声称「无新增警告」（report.md#L61-L62）。教训：警告基线必须逐条对比后才能
   下结论。
6. SuspendDVFS bEnabled 先清后调（E6）：SuspendDVFS() 在调用 framework 前先设
   bEnabled=false，framework 失败可能已经增加 suspend_count；失败路径必须调用
   ResumeDVFS() 恢复 flag 并平衡计数（report.md#L17-L18）。教训：失败回滚必须平衡
   全部副作用，不止电源状态。（并入自 R11-other 版，dsh 裁定核实属实）
7. 历史 026 同名区分（E7）：历史 026-inactive-crtc-vblank-guard.patch 保持不变，
   两个 026 以完整文件名区分；display 025 继续 UNVERIFIED（report.md#L28、#L76）。
   教训：patches/ 编号全局唯一（R12 亦验证）。（并入自 R11-other 版，dsh 裁定核实属实）
   这里的“全局唯一”是后续命名要求；既有同号件按完整文件名保留，不声称历史编号从未冲突。

## 四、怎么解决的

dsh 监督裁决（report.md#L222-L239）：「阶段 2 失败成立，处置正确，新根因方向批准。
独立核验：Server Errors 0→1、PowerLock POWERED_OFF 再次先于 PostPowerState(0→1)、
随后 hal_temperature_monitor_work 的 "recover gpu pll" 行、回退后 verify-install-
status PASS、mem_sleep=[s2idle] deep、kprobe/RTC 零残留、门禁全绿、git 0/0 干净……
新根因认定：patch-026 只关闭 devfreq 并发源；PCI 父设备 resume 在 innogpu_pci_drv.c
中恢复硬件后立即 hal_power_wakeup()（delay 0），温度监控 work 在子设备
PVRSRVDeviceResume() 完成前经 PLL 恢复的 PRE 回调进入 PVRSRVDevicePreClockSpeedChange
——同一 PowerLock 窗口的另一入口。方向批准：(a) 先关闭构建器 4.0.2-i1 默认入口并
同步 tracked 状态文档；(b) patch-027 把 resume 路径的温度 work 启动延后到 PVR resume
成功后（或等价明确门禁）……；(c) 版本 4.0.2-i2 = 024+026+027（025 仍排除）；(d) 控制器
round ID 规范化与 finalize 端到端 fixture 补齐。R11 无 tracked 变更，无需提交；结论：
deep 仍未修复，P3 保持打开，4.0.0-i1 为非 deep 基线。」（注：patch-027 编号随后在
R12 被 dsh 裁定改为 028，见 R12 叙事。）

## 五、特别说明

- 冻结措辞纪律：deep 仍未修复、P3 保持打开；4.0.2-i1 不得交付，也不得继续连续/
  供电/外屏矩阵；不得以「已修复」覆盖。
- 历史命令不构成执行授权：报告中的 deep 冒烟、回退与重装命令均为历史记录，不构成
  执行授权；下一次 deep 必须重新授权。
- 边界纪律：阶段 2 无 tracked 变更；drivers/、baselines/、binary-manifest.json、
  监督分支零改动；不热切、不改 dotfiles、不做 tag/Release；license/1C 不变。
- 后续影响：下一候选 4.0.2-i2 应把 hal_power_wakeup() 延后到 PVR resume 成功后（或
  等价明确门禁），保留 suspend 侧同步 cancel/flush、覆盖 resume 失败不启动 work、
  多设备和旧内核路径；display 025 继续 UNVERIFIED/NOT_SELECTED。
