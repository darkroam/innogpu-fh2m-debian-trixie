# 当前待办

本文件是未完成工作的唯一权威清单。当前运行结论见
[`status.md`](../project/status.md)，已完成工作与时序记录见 [`todo.md`](todo.md)。

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

- [ ] **suspend/resume 候选验收**：R10 在 `4.0.1-i3` 上复现 deep PowerLock POWERED_OFF，证明
  patch-024 的锁外快速门禁存在 TOCTOU；设备已回退 `4.0.0-i1`。R11 的 `4.0.2-i1` 以
  patch-024 + patch-026 lifecycle 在 PVR 下电前 drain devfreq、上电成功后再恢复，并补齐失败
  回滚和失败轮次 finalize fixture。当前只允许静态/离线构建验证；经 dsh 审查提交且用户本人在场
  重新确认后，才执行一次 deep 冒烟。P3 保持打开。`4.0.1-i4` 的 display 025 仍为独立
  UNVERIFIED 实验，不进入 i1。见 [`024-suspend-resume.md`](../patches/024-suspend-resume.md)、
  [`026-suspend-resume-dvfs-lifecycle.md`](../patches/026-suspend-resume-dvfs-lifecycle.md) 和
  [deep 事故](../incidents/suspend-resume-deep-reproduction-20260902.md)。
- [ ] **登录后短暂黑屏**：在 `4.0.1-i1` 和回退后的 `4.0.0-i1` 均复现，排除 patch-024 特异回归。
  合盖登录时 xdisplay 把初始 Xorg 布局切为 `EXTERNAL_ONLY`，Xorg 重建 1920x1080 framebuffer 并在约
  5 秒内重复查询输出，与“桌面亮一下、黑几秒、再恢复”时间窗一致。该问题归 dotconfig/xdisplay
  会话布局轮次处理，本仓库只保留设备接入事实；修复前不得在线试错 modeset。
- [ ] 将可复现的热点、perf 数据和应用级 workaround 整理为上游/厂商修复报告。
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
