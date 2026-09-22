# R04 · 显示恢复研究（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R04-2026-09-02-suspend-resume显示恢复研究`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R04-2026-09-02-suspend-resume显示恢复研究/narrative.md`，内容 SHA-256：
  `d4a89406a46b896c99baf50fec954b077d83c4d47c81055be66c9154e785f8da`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E4 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R04.md @ d78c79c429f3`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R04/R04-originals.tar @ a8edf619f70e`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮挂起冻结条款维持——新候选 + 复审后的回退与 watchdog 方案 + 人工在场
  （request.md#L48-L50 本轮禁止一切挂起/唤醒、模块热切与真机安装操作）

## 一、解决什么问题

R03 已确立：patch-024 消除了原 PowerLock 时序缺陷（真实 s2idle 无 3900372/PowerLock
失败），但唤醒后外屏整屏红色——显示恢复路径（KMS/CRTC/connector/plane）未闭合。本轮
研究「为什么不行」：给出有证据的根因假设 + 否定性证据；据此设计新补丁并重建 4.0.1
系列候选（4.0.1-i2，禁止复用失败候选 4.0.1-i1）。

## 二、怎么干的

先做根因研究（复核 R03 三份日志 11:55:10–11:55:31 唤醒窗口；追踪 innodpu_drm_resume()
→ innodpu_drm_wakeup() → drm_atomic_helper_resume() 与 pdp0_crtc_atomic_enable_legacy()
调用链；对 vendor 固定厂商对象做只读 readelf/objdump 反汇编）→ 形成候选根因（atomic
helper 回放完整状态后，厂商 post-atomic hook 又用保留的 hwdev 光标字段重复编程，可能
破坏刚提交的显示状态——有调用链和反汇编支持的假设，不是已证实因果）→ 否定性证据四条
→ 新增 patches/025-suspend-resume-display.patch（从 innosrvkm/innodpu_drm_pm.c 删除
atomic resume 之后的 innodpu_pdp0_wakeup() 遍历）→ 重建 4.0.1-i2（epoch 1788364800）
→ 双构建字节一致。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R04-2026-09-02-suspend-resume显示恢复研究/report.md#L5-L50 @ 7d48a1fe655c`（根因研究/否定性证据/解决方案与重建）
- 本机来源（非 Git、公开检出不可取得）：`collab/R04-2026-09-02-suspend-resume显示恢复研究/report.md#L112-L130 @ 7d48a1fe655c`（dsh 终审）
- 本机来源（非 Git、公开检出不可取得）：`collab/R04-2026-09-02-suspend-resume显示恢复研究/request.md#L19-L58 @ 4032143ecc2a`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R04/R04-originals.tar @ a8edf619f70e`（原文快照）
- [025-suspend-resume-display.patch](../../../patches/025-suspend-resume-display.patch)；历史版本锚：`patches/025-suspend-resume-display.patch#L1 @ 3aafaa93634b`
- [025-suspend-resume-display.md](../../patches/025-suspend-resume-display.md)；历史版本锚：`docs/patches/025-suspend-resume-display.md#L1 @ 3aafaa93634b`
- 事实值（非指针）：4.0.1-i2 epoch 1788364800；两次独立构建 SHA-256 均为
  b26c0b27…5e23a14；suspend 18/18、license 50/50、collab 26/26、package 9/9、
  全套 16 入口 406/406（unit 385 + 其他 21）

## 三、发现了什么问题（错误资产）

1. 首次 i2 构建失败（E1）：-Werror 下删除 CRTC 遍历后留下未使用的 drm_dev，编译失败；
   补丁同步删除两个不再使用的局部变量后，重新完成两次独立构建，不改变设计作用域
   （report.md#L87-L89）。教训：纯删除补丁必须连带清理因此失效的局部变量，否则编译
   门禁拦截。
2. patch-025 编号复用（E2）：与历史 patch-025-dma-resv-usage-rw.patch 同名共存，是
   request 指定的文件名；文档与测试均用完整名称区分，本轮不重命名历史补丁
   （report.md#L100-L101）。教训：编号命名空间冲突必须显式登记并用完整文件名区分。
3. 根因假设未证实（E3）：候选根因是调用链+反汇编支持的假设，不是已证实因果；公开
   检索未找到 FH2M 或同一黑盒 hook 的直接可移植红屏修复，没有冒充「社区已证实修复」
   （report.md#L20-L22、#L90-L92）。教训：假设与证据分层表述，仍需受控真机 A/B。
4. 免重启验证不可行（裁决 E4）：修复位于内核模块，运行新代码必须换模块或重启；热切
   模块被仓库纪律禁止且设备状态残留会污染 resume 验证、锁死风险反而高于有序重启。免
   重启只能做兼容性预检（vermagic/modversions 符号 CRC 与运行内核一致、DKMS 对当前
   内核头构建通过），已建议并入下一轮 pre-flight（report.md#L124-L127）。
   教训：预检只能证明兼容性准备，不能替代新模块实际运行后的恢复验证。

## 四、怎么解决的

修复方案：保留 patch-024 不变，patch-025 只删除 atomic resume 之后的
innodpu_pdp0_wakeup() 遍历；GEM、atomic resume、DPU/HDMI、fbdev 和 polling 恢复顺序
不变；drivers/ 源码树未修改。dsh 终审（report.md#L112-L130）：「独立核验：patch-025
为 11 行纯删除（移除 atomic resume 后的 innodpu_pdp0_wakeup() 遍历与两个失效局部变量）
……根因假设有调用链+反汇编支持且明确标注"未经真机 A/B 证明"，否定性证据完整，无冒充
社区已证修复。……门禁全绿：suspend 18/18、license 50/50、collab 26/26、package 9/9、
全套 16 入口 406/406、PASS_DOCS、审计 PASS/BLOCKED/CLEARED/BLOCKED……下一轮（R05）：
重启式受控 A/B——三份 4.0.0-i1 回退包复核、外部通道、重启安装 i2、非挂起显示基线、
s2idle A/B、deep 仅在全部通过且用户在场时执行。」

## 五、特别说明

- 冻结措辞纪律：i2 只是候选实验，不是可安装默认版；真机因果仍未验证，不得表述为
  「已修复」。
- 历史命令不构成执行授权：本轮按边界未执行挂起、安装、热切模块或显示状态修改；报告
  中的构建/预检命令均为历史记录。
- 边界纪律：drivers/、baselines/、binary-manifest.json 零改动；debs/vendor/build 只读；
  不做 tag/Release；license 状态与 1C 不变。
- 后续影响：下一轮必须补齐 resume 前后 DRM debugfs state、CRTC/plane FB ID、cursor
  enable/尺寸/位置及相关寄存器证据；人工画面、SSH、TTY 与 PVR/PowerLock 门禁必须同时
  通过（report.md#L96-L99）。
