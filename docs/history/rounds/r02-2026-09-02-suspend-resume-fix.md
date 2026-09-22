# R02 · 挂起恢复缺陷修复（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R02-2026-09-02-suspend-resume缺陷修复`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R02-2026-09-02-suspend-resume缺陷修复/narrative.md`，内容 SHA-256：
  `bd40310e2aedfb8a1393d8d272d9dd1f8f4f6934f984ca2db46eed0ba7e44e7c`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 保留已有事件的事实、处置与教训，不另改写结论；移除末尾私有抽验表，公开正文可独立阅读。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R02.md @ 8792fcc9161a`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R02/R02-originals.tar @ 34ae282b13e0`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮无 R5 冻结项；本轮生成的遗留冻结条款为「deep 真机验收必须准备物理恢复和
  已验证回退包，并由外部通道核对屏幕、SSH、TTY、journal 3900372/PowerLock 与 PVR 错误
  计数。不得在活动图形会话热切模块」（report.md#L88-L89）

## 一、解决什么问题

承接 dotfiles 项目 R08 交接（错题本 P3）：innogpu 驱动 suspend/resume 缺陷导致 deep(S3)
唤醒黑屏——屏幕不亮、SSH 不可达、TTY 无画面、键盘盲输无效，仅电源键有效（userland 存活、
GPU/显示栈死亡），100% 复现于唤醒瞬间。journal 关键行：PVR_K:(Error): 3900372:
PVRSRVEPowerLock() failed (PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF) in
PVRSRVDevicePreClockSpeedChange()（request.md#L21）。目标：修复 resume 电源状态机时序
（时钟切换应在设备上电完成后/受锁保护），交付可测试补丁与验证方法；并验证 s2idle 是否
可作短期规避。

## 二、怎么干的

先只读复核根因与事故（当前运行 4.0.0-i1，本轮不构建/安装/加载补丁）→ 新增
patches/024-suspend-resume.patch（只改 innosrvkm/pvr_dvfs_device.c：OPP 查询与
PVRSRVDevicePreClockSpeedChange() 前读取 PVRSRVDefaultDomainPower()，非 ON 时保持当前
核心频率并返回，ON 保留原锁与错误传播）→ 新架构候选独立升号 4.0.1-i1（epoch 1788278400）
→ legacy 对照包版本 patched-28 → 静态 fixture 与 CI 接线 → 事故/补丁/状态文档全量标注
候选未构建、未安装、未实机验证 → 停在提交前交 dsh 审查。

历史产物与来源：

- 本机来源（非 Git、公开检出不可取得）：`collab/R02-2026-09-02-suspend-resume缺陷修复/report.md#L5-L44 @ 0f5e42b41fff`（根因复核/候选修复/公开参考）
- 本机来源（非 Git、公开检出不可取得）：`collab/R02-2026-09-02-suspend-resume缺陷修复/report.md#L108-L121 @ 0f5e42b41fff`（dsh 终审）
- 本机来源（非 Git、公开检出不可取得）：`collab/R02-2026-09-02-suspend-resume缺陷修复/request.md#L41-L88 @ d45dae7f5ae3`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R02/R02-originals.tar @ 34ae282b13e0`（原文快照）
- [024-suspend-resume.patch](../../../patches/024-suspend-resume.patch)；历史版本锚：`patches/024-suspend-resume.patch#L1 @ 9908bda6b4ed`
- [024-suspend-resume.md](../../patches/024-suspend-resume.md)；历史版本锚：`docs/patches/024-suspend-resume.md#L1 @ 9908bda6b4ed`
- [suspend-resume-deep-reproduction-20260902.md](../../incidents/suspend-resume-deep-reproduction-20260902.md)；历史版本锚：`docs/incidents/suspend-resume-deep-reproduction-20260902.md#L1 @ 9908bda6b4ed`
- 事实值（非指针）：当前运行包 4.0.0-i1；候选 4.0.1-i1 未构建；静态套件 suspend 12/12、
  version 7/7、license 50/50、collab 26/26、全量 16 入口 400/400；HEAD=origin/main=301d62c

## 三、发现了什么问题（错误资产）

1. s2idle 测试方法缺陷（E1）：本轮唯一的真机尝试方法有缺陷——systemctl suspend 约
   17 ms 即返回，EXIT trap 随即把 /sys/power/mem_sleep 恢复为 deep，而内核约 2 秒后才
   真正进入挂起；journal 明确记录 PM: suspend entry (deep) 和 ACPI S3，最终再次触发旧
   驱动故障并只能重启（report.md#L13-L17）。处置：事故后按用户要求停止所有电源、安装和
   模块操作，真机阶段移出本轮；s2idle 实际没有被测试，不记 PASS、FAIL 或可规避。教训：
   挂起模式必须以内核真正入睡时刻的状态为准，journal 的 PM: suspend entry 行是唯一
   可信判据。
2. 补丁编号漂移（E2）：提示词称当前最新编号 023，但仓库已有 patch-025/026/027，024 仍
   空缺；而历史包版本 patched-24 已被占用，legacy 对照改用 patched-28（report.md#L27-L29、
   #L76-L77）。处置：补丁文件用 024，包版本用 patched-28，分别登记。教训：补丁编号与
   包版本是两个独立命名空间，引用前必须实测仓库现状。
3. 版本升号要求（E3）：用户要求源码变化必须升号，正式候选从 4.0.0-i1 独立升为 4.0.1-i1；
   patched-28 仅作旧链对照（report.md#L78-L79）。教训：候选包不得与已验证运行包同号共存。
4. 静态通过不等于修复通过（E4）：因继续禁止读取 debs/、vendor/、build/，候选未构建；
   全库页面把 4.0.1-i1 标为未构建、未安装、未实机验证候选，禁止冒充当前运行版本或已
   修复状态（report.md#L32-L33、#L80-L81）。教训：门禁全绿只是静态证据，不构成真机
   修复结论。

## 四、怎么解决的

修复语义：devfreq OPP/PreClock 前检查 PVR 默认电源域，非 ON 时保持当前频率早退（不访问
OPP/电压/时钟），ON 状态保留原有锁和错误传播；构建器同时把 patch-024 应用到离线编译
staging 与包内 DKMS 源码，避免同版本不同源码。dsh 终审（report.md#L108-L121）：
「独立核验：patch-024 对 drivers/innosrvkm/pvr_dvfs_device.c 干净应用（dry-run 实测）；
语义为电源域非 ON 时跳过 OPP/时钟/电压访问并保持当前频率……版本控制属实：当前运行版
4.0.0-i1 与候选 4.0.1-i1（epoch 1788278400）表述全库诚实，无冒充已修复/已安装。门禁
全绿：license 50/50、collab 26/26、suspend 12/12、version 7/7、package 9/9、全套 16
入口 400/400、PASS_DOCS、审计 PASS/BLOCKED/CLEARED/BLOCKED……按 §七 由 dsh（监督）
批准提交。」遗留五项（4.0.1-i1 真机构建/安装/重启验收；deep 验收回退包与外部通道；
s2idle 测试方法重设计；停提交前；P3 不关闭）见 report.md#L85-L93 与 #L118-L120。

## 五、特别说明

- 冻结措辞纪律：4.0.1-i1 为「候选修复待验证」，P3 必须保持打开；不得以「已修复/已
  通过」类措辞覆盖 deep 未验证状态。
- 历史命令不构成执行授权：request/report 中的构建、安装、DKMS、挂起等命令均为历史
  记录，未获新授权不得执行；本轮结论停在提交前，未提交、未推送、未创建 tag/Release。
- 边界纪律：drivers/、baselines/、binary-manifest.json 与监督分支零改动；license 状态
  与发布决策 1C 不变。
- 回传 dotfiles 摘要见 report.md#L94-L103：候选修复待验证，P3 不能关闭。
