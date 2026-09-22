# R08 · primary FB 与 DPU 观测（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R08-2026-09-02-suspend-resume-primaryFB与DPU观测`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R08-2026-09-02-suspend-resume-primaryFB与DPU观测/narrative.md`，内容 SHA-256：
  `d3cf7d481956a9bdb2bcc50b462a963d4f97e715aacf164819234ac27629f8ef`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 保留已有事件的事实、处置与教训，不另改写结论；移除末尾私有抽验表，公开正文可独立阅读。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R08.md @ 3dd6d1a71831`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R08/R08-originals.tar @ 785960e05963`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

假设 2/4 均未证实也未排除；patch-025 保持 UNVERIFIED；deep 冻结不变
  （report.md#L84-L88）

## 一、解决什么问题

R07 证明常规 modesetting 桌面 cursor 分支天然不入组；用户决策转向 primary FB/GEM 与
DPU shadow/config-valid 只读观测（次优假设 2/4 方向）。本轮扩展观察器、完成 R07 裁决
要求的硬包装器端到端采样（不依赖图形 askpass）、建立非挂起基线，并设计一次性 i3
s2idle 复现计划（仅设计，不执行）。

## 二、怎么干的

扩展 R07 只读观察器至 27 个探针（primary plane 的 framebuffer/GEM 指针/ID/格式/尺寸/
pitch/自然设备地址返回、plane update/GEM free、DPU enter/leave/config-valid 调用与
自然 HAL 写入）→ 包装器改为从已加载 /sys/kernel/btf/innogpu 校验运行 ABI（对
drm_crtc.state=1480、drm_crtc_state.active=9 等偏移失败关闭）→ 纠正三项证据表达错误
→ 两组 root 整包装器非挂起采样（27 probes attach、r08_wrapper_e2e=PASS、
r08_observer_result=PASS）→ 与 R07 样本对照并收束结论 → 形成一次性 i3 s2idle 复现
计划（未执行）。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R08-2026-09-02-suspend-resume-primaryFB与DPU观测/report.md#L5-L57 @ 988c139ceaf2`（扩展/纠正/两组采样）
- 本机来源（非 Git、公开检出不可取得）：`collab/R08-2026-09-02-suspend-resume-primaryFB与DPU观测/report.md#L82-L100 @ 988c139ceaf2`（结论与复现计划）
- 本机来源（非 Git、公开检出不可取得）：`collab/R08-2026-09-02-suspend-resume-primaryFB与DPU观测/report.md#L114-L127 @ 988c139ceaf2`（dsh 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R08-2026-09-02-suspend-resume-primaryFB与DPU观测/request.md#L7-L45 @ ed27ee2cdee9`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R08/R08-originals.tar @ 785960e05963`（原文快照）
- [probe-suspend-resume-observer.bt](../../../tools/probe-suspend-resume-observer.bt)；历史版本锚：`tools/probe-suspend-resume-observer.bt#L1 @ 28729b3cf1e9`
- [probe-suspend-resume-state.sh](../../../tools/probe-suspend-resume-state.sh)；历史版本锚：`tools/probe-suspend-resume-state.sh#L1 @ 28729b3cf1e9`
- 事实值（非指针）：27 probes attach PASS；suspend 30/30、license 50/50、collab 26/26、
  16 入口 422/422（unit 399 + 其他 23）

## 三、发现了什么问题（错误资产）

1. 三项证据表达错误（E1）：HAL 第二、三个参数按原型记为 reg_module/reg_entity，不再
   把枚举值 15 写成 dpu=15；entry、完整 return 与 unmatched_at_stop 分开统计；primary
   plane 名称去除多余冒号且解析不越过 plane 区块（report.md#L13-L15）。处置：交付前
   字段修正并新增两个静态反例，用例 28→30（report.md#L76-L78）。教训：汇总标签与
   计数必须与调用原型逐字段对齐，错误标签不得进入结论。
2. R07 空 crtc->state 的根因纠正（E2）：已确认是用 shipped-object DRM 偏移 1176 读取
   运行对象造成的误判；当前加载模块偏移为 1480。历史 R07 报告未改写，纠正结论写入
   tracked 文档（report.md#L19-L21）。dsh 认可（report.md#L120-L122）：「运行模块 BTF
   偏移 1480 取代 shipped-object 偏移 1176（R07 空 crtc->state 误判的根因），历史 R07
   报告未改写、纠正写入 tracked 文档，符合"不改写历史"纪律。」教训：ABI 偏移必须以
   运行模块为准，shipped-object 与运行对象可能不同。
3. dmenupass 再度阻断（E3）：清理未使用计数 map 后，dmenupass 曾因显示切换无法 grab
   keyboard，sudo 在执行前退出且无残留；随后用户在本机终端对最终文件执行 sudo
   bpftrace --dry-run，再次得到 Attaching 27 probes...，最终 probe 语法与 attach 门禁
   闭环（report.md#L54-L57）。教训：机器人在场的自动化入口不可依赖图形提权；人工
   终端兜底须保留证据。
4. 采样停止时 entry/return 数不等（E4）：已完成的 114 条 FB/GEM/scanout return 地址链
   一致；entry 多于 return 的差值报告为 unmatched_at_stop，证据质量为
   PARTIAL_UNMATCHED_AT_STOP，不用于判断内核调用仍在执行（report.md#L49-L51）。
   教训：异步采样必须以窗口边界标记证据完整性等级。

## 四、怎么解决的

本轮结论（report.md#L84-L88）：正常桌面完整调用链内 primary FB/GEM 设备地址与最终
scanout 地址一致——这是健康基线，只削弱「正常运行时持续地址错误」，不能外推到
suspend/resume 故障窗口；没有 shadow bank 安全 readback，不能据此排除恢复阶段的时序
错误；假设 2/4 均未证实也未排除；cursor 分支仍未自然入组，patch-025 保持 UNVERIFIED；
deep 冻结不变。dsh 裁决（report.md#L114-L127）：通过，批准提交；门禁全绿（suspend
30/30、license 50/50、collab 26/26、422/422、PASS_DOCS、审计
PASS/BLOCKED/CLEARED/BLOCKED）；「结论口径认可：健康基线只削弱"持续地址错误"假设，
假设 2/4 均未证实未排除；025 保持 UNVERIFIED；deep 冻结不变。」

## 五、特别说明

- 冻结措辞纪律：健康基线不得外推为故障窗口结论；patch-025 与 deep 均保持冻结措辞，
  不得以「已通过」覆盖。
- 历史命令不构成执行授权：一次性 i3 s2idle 复现计划（report.md#L90-L100）仅为设计，
  执行须用户在场 + 用户/dsh 再次明确批准；报告中的采样/挂起命令均为历史记录。
- 边界纪律：只读、不 modeset、不挂起、不热切；drivers/、baselines/、
  binary-manifest.json 零改动；license/1C 不变。
- 后续影响：R07 与 R08 使用不同显示器/分辨率，不比较数值频率；若获单独批准，只在
  i3 做一次带 RTC、300 秒 watchdog 和 observer 的 s2idle 复现，未复现或异常即停，
  deep 禁止。
