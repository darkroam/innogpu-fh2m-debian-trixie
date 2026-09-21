# 当前待办

本文件是未完成工作的唯一权威清单。当前运行结论见
[`status.md`](../project/status.md)，已完成工作与时序记录见 [`todo.md`](todo.md)。

## R5 悬案与 fantgpu 5.0.0 主线（当前最高优先级）

- [ ] **R5 挂起根因定位**：fantgpu 绑定下 `pm_test=devices` 硬挂；两轮诊断内核（r5dpm1/r5dpm2）
  复核判定 `OUTSIDE_COVERAGE`，根因未定位。冻结：禁止重跑任何 pm_test/watchdog；下一方向待用户
  选择（停批冻结 / 本地扩展轮——dpm_prepare 前置段 instrumentation 或 O-vs-F 继续反编译 / 带外
  通道）。证据链：
  [r5 调查计划](../design/r5-suspend-investigation-plan.md)、
  [步骤 8 结果](../planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-step8-result.txt)。
- [ ] **发布阻断（fantgpu 5.0.0-iN 线）**：① `postinst_current_kernel_only=release_blocker`（postinst
  只构建当前运行内核）；② `validation-results.json` 未签；③ tag `fantgpu-5.0.0-iN` 未打；
  ④ R5=FAIL 未解除。许可发布边界（1C/BLOCKED）不变。
- [ ] 诊断内核处置：r5dpm1/r5dpm2 包保留待 dsh 决定是否卸载；GRUB 已恢复原配置（默认解析
  6.12.107+deb13，既有行为；改默认须另立变更）。
- [ ] **R17 文档优化迭代（第 4 轮收尾中）**：第 1 轮内容对齐（collab 叙事批 R01-R16 +
  docs 对齐 + incidents 三类提升）已闭合；第 2 轮三基线代际文档已提交（1bf294a，
  含 R2-F1 更正：patched-27 deb 在 `debs/` 实物一致 f384159751fe）；第 3 轮二次
  对齐已闭合（复审台账 round3-review-ledger.md）；第 4 轮收尾进行中（跳回台账为空）。

## 许可证与研发验证

- [ ] **发布阻断：权利链人工审查**。3 个 `Strictly Confidential` 文件与 70 个无许可文件已从公开
  制品排除（不从 Git 历史删除）；driver-source 制品因排除后无法独立构建且 408+2 个已有声明文件
  的授权链未闭合保持 BLOCKED（不假 PASS）；192 项 manifest 载荷权利链**仅在计划公开二进制制品时**
  需要，当前不作为发布目标。关闭 `license_release_gate=BLOCKED` 前不得发布完整源码树或载荷附件。
- [ ] 完成研发验证矩阵：扩展坞/多屏/无盖桌面/其他机型，以及电源/合盖场景。该项用于提高研发结论的外推性，不是当前 release 工作。

**非活动条件项**：发布决策 1C 规定当前不创建 GitHub Release、tag 或发布附件，
`main` 不作为发布目标。只有用户未来明确推翻 1C 后，才能重新激活
`source-v4.0.0-i1` annotated tag、release 附件审查和 Phase 5 第二步的发布周期前置；
在此之前不得把规划名写成现有发布标识。权威决策见
[`licensing.md` §4.1](../project/licensing.md#41-github-主分支发布面与发布决策-1c当前结论)。

## 维护与包生命周期

- [ ] 为每次新候选包建立独立的 `docs/patches/` 说明和 `docs/incidents/` 验收记录。
- [ ] 将长期维护所需的脚本参数逐步收敛为可审查的配置，保持 `scripts/<name>` 兼容入口不变。
- [ ] 补齐音频安装器的写入冲突/备份保护、`systemd-analyze verify`、对称卸载与 fixture，并明确用户服务管理失败策略。
- [ ] 为最小化 Debian 环境补齐新构建器前置依赖门禁（至少显式核对/安装 `python3` 及当前直接调用的
  dpkg/coreutils/kmod 工具），避免把 `install-prereqs-debian.sh` 成功误认为完整构建工具链可用。
- [ ] 明确包内 vendor `sw-inno-gl.service`/`sw-inno-gl` 的保留与生命周期策略；若保留，补齐 control
  依赖、enable/start/卸载边界，并让 release gate fixture 校验 unit、helper 及 10 个 `/usr/bin`+
  `/usr/sbin` 稳定命令链接；若移除，更新 manifest 与包载荷审计后再构建新版本。

## 运行时、测试与上游报告

- [x] **suspend/resume 正式验收**：R10 在 `4.0.1-i3` 上复现 deep PowerLock POWERED_OFF，证明
  patch-024 的锁外快速门禁存在 TOCTOU。R11 的 `4.0.2-i1` 增加 patch-026 同步 devfreq/PVR
  生命周期后仍在 deep 失败：独立温度 work 从父 PCI resume 零延迟启动，早于 PVR 子设备恢复。
  i1 只保留历史复现且不得安装/交付；设备曾回退 `4.0.0-i1`。R12 的 `4.0.2-i2` 以 patch-028
  把 work 延后到全部 PVR 子设备与 DVFS 恢复成功后；R13 的 `4.0.2-i3` 再以 patch-029
  让 DDCCI 回退模式创建 panel 但不注册 backlight device。R13 阶段 2 通过，R14 随后完成
  D1-D6 共 6/6 deep 正式矩阵（接电/电池、无外屏/外屏），当前设备正式交付 `4.0.2-i3`。
  P3 回传建议为“已修复，待 dotfiles 复核”；display 025 仍为独立 UNVERIFIED 实验，不进入
  i1/i2/i3。DDCCI 无亮度控制，`hwinfo_g0m.bin` 仍缺失。
  **2026-09-20 对齐注**：R16 起主线已切至 fantgpu 血缘（5.0.0-iN，当前 i6 诊断线）；本条
  deepin 血缘结论作为历史与回退基线保留，4.0.2-i3 现为回滚卡（SHA `177133ee…`）。见
  [`026-suspend-resume-dvfs-lifecycle.md`](../patches/026-suspend-resume-dvfs-lifecycle.md)、
  [`028-suspend-resume-hal-temp-monitor-delay.md`](../patches/028-suspend-resume-hal-temp-monitor-delay.md)、
  [`029-suspend-resume-ddcci-panel.md`](../patches/029-suspend-resume-ddcci-panel.md) 和
  [deep 事故](../incidents/suspend-resume-deep-reproduction-20260902.md)。
- [ ] **登录后短暂黑屏**：在 `4.0.1-i1` 和回退后的 `4.0.0-i1` 均复现，排除 patch-024 特异回归。
  合盖登录时 xdisplay 把初始 Xorg 布局切为 `EXTERNAL_ONLY`，Xorg 重建 1920x1080 framebuffer 并在约
  5 秒内重复查询输出，与“桌面亮一下、黑几秒、再恢复”时间窗一致。该问题归 dotconfig/xdisplay
  会话布局轮次处理，本仓库只保留设备接入事实；修复前不得在线试错 modeset。
- [ ] 已知偶发 fixture（pre-existing，待单独排查）：exec-probes / VA-API / DMA-BUF
  在负载下偶发 1–2 项失败（qoder 初审与 dsh 复跑均见过两种状态），非 028 回归；
  排查前 CI 结果解释需注意"偶发"口径。
- [ ] 将可复现的热点、perf 数据和应用级 workaround 整理为上游/厂商修复报告。
- [ ] **R15 启动告警上游/厂商报告**：整理 `hwinfo_g0m.bin` 未随厂商载荷分发导致的
  hwinfo 注册失败、DP/HDMI/VGA 默认降级与 DDCCI fallback，以及 `dmaengine` 下 AXI
  DMA/PCIe DMA 同设备名冲突；AMD-Vi、microcode、SRSO、ACPI `_DOD/_DOS` 另列为平台/BIOS
  侧事项。详见 [启动报错归因记录](../incidents/boot-errors-attribution-20260903.md)。
- [ ] runtime 剩余真实能力证据：modeset/热插拔/合盖、Picom GLX backend、
  音频听感确认；当前权威汇总 22 PASS / 9 SKIP / 4 UNVERIFIED。
- [ ] VA-API 未测 profile（H.264 High/Constrained Baseline、HEVC Main10）、编码能力和多屏矩阵继续
  按独立能力项补证据。
- [ ] 构建失败用例补齐（headers 缺失、helper 缺失、SOURCE_DATE_EPOCH 缺失）为 fixture。

## 逆向工程与能力挖掘

- [ ] CORE_ID/BVNC 直接读取验证。
- [ ] 私有 `libinno_codec.so` 编码接口验证。
- [ ] invisible READ 批量预取候选调研（调用方批量化，不修改 `innodma.o_shipped` 内部；先补设计）。
- [ ] DVFS/功耗实测与调参评估（候选 7）。
- [ ] `inno_apphint.c` 用户态调优评估（候选 5）。
- [ ] 上游 DDK bugfix/性能 patch 移植（候选 6，依赖开源 DDK 可得性）。
- [ ] 用户态调用画像：扩展 `trace-loader.c` 到 GL/VK/OCL 路径（候选 8）。
- [ ] 完成 `innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）符号级分析，评估预编译核心替换路径（远期定向 RE）。
