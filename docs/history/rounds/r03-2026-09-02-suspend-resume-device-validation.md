# R03 · 挂起恢复真机验收（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R03-2026-09-02-suspend-resume真机验收`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R03-2026-09-02-suspend-resume真机验收/narrative.md`，内容 SHA-256：
  `d6ee631face0d9658e3b26703c9e962660edcfe8cd16433b58d2e5e513c263ce`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E5 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。
6. 移除一处具体 boot UUID，以“本机 boot 标识已省略”替代；事件、时间窗与判定不变。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R03.md @ d18f6cee7161`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R03/R03-originals.tar @ 2a34c99acee8`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮起挂起冻结维持——「新候选 + 复审后的回退与 watchdog 方案 + 人工在场，
  方可再次挂起」（report.md#L138）

## 一、解决什么问题

承接 R02 遗留，在真机上完成 patch-024 / 4.0.1-i1 的构建、安装与挂起验收：判定修复是否
有效，给出 s2idle 规避结论与 dotfiles 错题本 P3 回传文本。分阶段 A（构建安装回退准备，
不挂起）与阶段 B（B1 s2idle 先做、B2 deep 后做，每步前置门禁）。

## 二、怎么干的

阶段 A：复核三份 4.0.0-i1 回退包 SHA 一致 → 构建 4.0.1-i1（epoch 1788278400）→ 离线
DKMS/包边界/包内补丁检查 → 包安装重启（不热切）→ 版本与基础图形机械门禁。阶段 B1：
mem_sleep=s2idle + 20 秒 RTC 唤醒，journal 独立确认 entry/exit 与 PVR 门禁，人工可见
画面为最终判据；失败则按提示词立即停止 B2。之后完整回退 4.0.0-i1，并对「登录后短暂
黑屏」做独立只读调查（不并入红屏归因）。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R03-2026-09-02-suspend-resume真机验收/report.md#L5-L61 @ a1a8e03f901a`（阶段 A/B1/关机回退/闪黑调查）
- 本机来源（非 Git、公开检出不可取得）：`collab/R03-2026-09-02-suspend-resume真机验收/report.md#L126-L139 @ a1a8e03f901a`（dsh 终审与冻结条款）
- 本机来源（非 Git、公开检出不可取得）：`collab/R03-2026-09-02-suspend-resume真机验收/request.md#L10-L61 @ 4bd253d2032f`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R03/R03-originals.tar @ 2a34c99acee8`（原文快照）
- [suspend-resume-s2idle-red-screen-20260902.md](../../incidents/suspend-resume-s2idle-red-screen-20260902.md)；历史版本锚：`docs/incidents/suspend-resume-s2idle-red-screen-20260902.md#L1 @ 81bbed78f16d`
- [024-suspend-resume.md](../../patches/024-suspend-resume.md)；历史版本锚：`docs/patches/024-suspend-resume.md#L1 @ 81bbed78f16d`（失败验收状态更新）
- 事实值（非指针）：回退包 SHA 68aea6c0…8735；候选 SHA a7fe10ed…27e6；boot ID
  [本机 boot 标识已省略]；回退后 4.0.0-i1、mem_sleep=s2idle [deep]

## 三、发现了什么问题（错误资产）

1. B1 s2idle 真实失败（E1）：journal 无 3900372、无 PVRSRVEPowerLock failed、PVR 计数
   不增长、SSH/自动化 Xorg-GL 探针恢复——但用户看到外屏整屏红色、显示不可用。用户可见
   结果优先，B1 判定 FAIL（report.md#L31-L33）。处置：取消 B2 deep，本轮永久停止后续
   挂起测试，完整回退 4.0.0-i1。教训：机械门禁干净不等于显示恢复，「人工可见画面」必须
   是硬门禁；s2idle 不是可用规避。
2. B2 deep NOT RUN（E2）：提示词要求任一中途异常立即停止；B1 已出现真实红屏，继续
   deep 风险不可接受（report.md#L34、#L93-L95）。教训：风险门禁优先于计划完整性。
3. 登录后短暂黑屏独立调查（E3）：4.0.1-i1 观察到的黑屏在回退 4.0.0-i1 后仍复现，排除
   patch-024 特异回归；证据指向合盖登录后 xdisplay 把 Xorg 初始布局切到 EXTERNAL_ONLY
   的真实 modeset（report.md#L47-L61）。处置：只读取证，不修改 dotfiles、不执行 xrandr
   写、不重启 Xorg，交给独立 dotconfig/xdisplay 轮次。教训：不同症状不得合并归因。
4. 证据目录纪律（E4）：/tmp 不是持久证据目录（重启后可能被清理）；Xorg 的 /tmp/
   serverauth.* 每次会话重新生成，旧路径不应复用；长期证据改存被忽略的 build/
   （report.md#L41-L42、#L109）。教训：重启敏感证据必须落在 gitignored 持久目录。
5. 失败候选构建器处置裁决（E5）：dsh 裁决不强制构建器拒绝 4.0.1-i1——保留仅供复现，
   文档已明确禁止安装；下一修复必须升新版本（report.md#L134-L135）。
   教训：保留历史构建入口不等于允许安装失败候选，版本推进与安装策略必须分别声明。

## 四、怎么解决的

dsh 终审（report.md#L126-L139）：「结论认定：patch-024 只闭合了 PVR devfreq/PowerLock
入口，未闭合完整显示恢复路径；s2idle 唤醒外屏红屏以人工可见结果判定 FAIL，处理正确。
B1 失败后取消 B2 deep 属于安全协议要求的停止，偏差理由充分，予以批准。……挂起冻结维持：
新候选 + 复审后的回退与 watchdog 方案 + 人工在场，方可再次挂起。」P3 结论：
「s2idle 不是规避，deep 未测试，候选已回退 4.0.0-i1」（report.md#L112-L121）。

## 五、特别说明

- 冻结措辞纪律：patch-024 未闭合显示恢复路径，不得以「已修复/已通过」覆盖；4.0.1-i1
  为失败候选（禁止安装、仅供复现）。
- 历史命令不构成执行授权：报告中的回退命令（完整 deb 安装 + 允许降级 + 重启）为历史
  记录，不构成执行授权。
- 边界纪律：drivers/、baselines/、binary-manifest.json、debs/vendor/build（除授权读取）
  零改动；不做 tag/Release；license 状态与 1C 不变。
- 后续影响：下一候选必须重新定位红屏时的显示恢复时序并升新版本；xdisplay 登录闪黑另
  开轮次；/tmp 不作重启后证据目录的约定成立并已入事故记录。
