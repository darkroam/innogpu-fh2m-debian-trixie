# R14 · deep 正式验收矩阵（2026-09-03，公开阅读版）

- 记录范围：来源轮次 `R14-2026-09-03-suspend-resume正式验收矩阵`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/narrative.md`，内容 SHA-256：
  `6911459b8c327b0640b1c7c504cc3596af03d21535fb5488975c5251a705da94`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E6 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R14.md @ 7e75ec49e368`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R14/R14-originals.tar @ 4a54dfc96917`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮结论只覆盖当前矩阵（当前设备/布局）下的 deep 恢复，不扩大为其他
  设备/布局的保证；DDCCI 无亮度控制、hwinfo_g0m.bin 缺失、patch-025 未包含
  （report.md#L104-L105）

## 一、解决什么问题

在已安装运行的 4.0.2-i3 上完成 dotfiles 转交要求的正式验收矩阵：3 次连续 deep +
拔电/接电 × 外屏/无外屏组合（D1-D6）；全绿后产出正式交付材料（版本/包名/SHA/升级
说明/脱敏验证记录）与 P3 状态更新建议。用户每轮本人在场。

## 二、怎么干的

按矩阵 D1-D3（接电/无外屏/合盖连续 3 次）→ D4（电池/无外屏）→ D5（接电/HDMI-2
外屏 1920x1080@60）→ D6（电池/HDMI-1 外屏）逐轮执行：每轮倒计时给用户完成供电/
外屏/合盖准备 → 自动读取前置状态 → RTC 90 秒触发 deep → 唤醒后人工确认画面、键鼠、
TTY（外屏轮加外屏画面）→ PVR 八项计数对比与 journal 判据。全绿后 codex 完成 tracked
文档收尾并提交。中途曾按用户指示暂停以插入 R15，随后恢复。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/report.md#L5-L33 @ 84473f484f55`（矩阵执行与轮次结果）
- 本机来源（非 Git、公开检出不可取得）：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/report.md#L35-L106 @ 84473f484f55`（判据/版本与 SHA/升级回退/P3 建议）
- 本机来源（非 Git、公开检出不可取得）：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/report.md#L139-L179 @ 84473f484f55`（dsh 终审与 codex 收尾提交）
- 本机来源（非 Git、公开检出不可取得）：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/request.md#L7-L41 @ aa2bfacbe784`（dsh 完整提示词与矩阵）
- 本机来源（非 Git、公开检出不可取得）：`collab/R14-2026-09-03-suspend-resume正式验收矩阵/qoder-notes.md#L8-L78 @ 798e9f84a9b0`（qoder 初审）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R14/R14-originals.tar @ 4a54dfc96917`（原文快照）
- [README.md](../../../README.md)；历史版本锚：`README.md#L1 @ b7372e3eb356`（4.0.2-i3 正式交付状态收尾）
- 事实值（非指针）：4.0.2-i3 SHA 177133ee…9e1662；回退包 SHA 68aea6c0…8735；
  D1-D6 全部 rtcwake_rc=0；收尾提交 b7372e3、push 后 HEAD=origin/main=b7372e3

## 三、发现了什么问题（错误资产）

1. D1 PIPESTATUS 误用（E1）：D1 控制命令在 RTC 唤醒后误用 zsh 的 Bash PIPESTATUS，
   导致自动收尾提前退出；RTC 和 deep 已完成，随后人工停止 observer 并补齐 post 证据。
   后续轮次改用无管道 RTC 调用并成功收尾（report.md#L77-L79）。教训：控制脚本的
   shell 方言差异（zsh vs Bash）须在真机运行前验证，收尾路径不能依赖单点管道状态。
2. D1 observer 越界记录（E2）：D1 原始 observer 因未及时退出继续记录后续轮次；已按
   首个 D1 suspend 到首轮 innodpu_pdp0_wakeup 完成边界提取 d1-function-trace-window.txt，
   原始累计文件保留并明确标注，未用于单轮结论（report.md#L80-L82）。教训：观测器
   生命周期必须绑定单轮窗口，越界数据必须边界切割并标注。
3. 前置条件中止（E3）：D2、D3、D6 各有一次倒计时后的前置条件中止：分别是状态拼接
   校验修正、用户检查后未重新合盖、用户检查后外屏断开且未合盖；均未进入 suspend，
   不计入正式轮次，也未触发回退（report.md#L75-L76）。教训：前置门禁拒绝入组是
   正确行为，中止轮次必须与正式轮次分开记账。
4. D5 快照笔误（E4）：D5 后置硬件快照有变量名笔误，未影响 deep 或显示；已补存
   post-hardware-corrected.txt，以修正文件作为 D5 硬件证据（report.md#L83-L84）。
   教训：证据文件命名错误要立即修正补存，不能以错误文件充当证据。
5. 未执行独立 SSH 取证（E5）：设备本地可见，用户在场完成所有人工判据，内核和 sysfs
   证据由主机权限直接保存（report.md#L85-L86）。处置：如实记录，不以本地状态冒充
   外部网络验证。教训：取证通道与判据声明必须一致。
6. 流程事件（E6）：用户临时暂停 R14 以插入 R15，随后按原矩阵恢复（report.md#L118-
   L132）；新规「提交决定权归作者」（report.md#L131-L132、#L155-L171）——codex 作者
   决定提交 b7372e3（docs: mark 4.0.2-i3 as validated delivery），dsh 终审无异议后
   push（report.md#L176-L179）。
   教训：暂停、恢复和提交责任变化应分别留痕，避免沿用过时的执行队列或角色授权。

## 四、怎么解决的

dsh 终审（report.md#L137-L151）：「采纳 qoder 初审（18/18）并独立抽验：6 个正式
轮次证据目录齐全（另有 3 个 precondition_failed 中止目录，与报告一致）、各轮 PVR
错误 grep=0、D5 deep 窗口 18:56:14→18:57:51、Git 0/0 干净。结论：4.0.2-i3 正式
验收矩阵 6/6 deep 全绿（接电/电池 × 无外屏/外屏），dotfiles 转交要求达成。交付版本：
4.0.2-i3（SHA 177133eebda692092501a27d7d135662ddaedaf3634776b8aa1ea5153c9e1662），
已知边界随附（DDCCI 无亮度控制、hwinfo_g0m.bin 缺失、025 未包含）。P3 回传建议：
"已修复，待 dotfiles 复核"。」codex 收尾：README/status/current-work/new-device-
install/patches 文档将 4.0.2-i3 更新为正式交付状态（含边界）；提交 b7372e3 后
PASS_DOCS、实时许可审计 PASS，未 push 前 Git 0/0；随后 dsh 终审执行记录确认 push。

## 五、特别说明

- 冻结措辞纪律：P3 回传建议为「已修复，待 dotfiles 复核」——是待外部复核的建议
  状态，不是本仓库单方宣告；诚实边界（DDCCI 无亮度控制、hwinfo_g0m.bin 缺失、
  patch-025 未包含且 UNVERIFIED）随交付同行；「本次只证明当前矩阵下的 deep 恢复，
  不扩大为其他设备/布局的保证」。
- 历史命令不构成执行授权：报告中的升级/回退命令（允许降级安装 + 重启 + 安装状态
  验证）均为历史记录，不构成执行授权；本轮全绿未执行回退。
- 边界纪律：R14 测试本身无 tracked 变更（仅收尾文档提交 b7372e3）；drivers/、
  baselines/、patches/ 零改动；不热切模块、不改 dotfiles、不创建 tag/Release；
  license/1C 不变。
- 后续影响：D5/D6 外屏连接器编号（HDMI-2/HDMI-1）按物理连接状态记录，不视为功能
  差异；D2-D6 无独立恢复 trace（仅 D1 完整 trace）为 qoder P3 观察项。
