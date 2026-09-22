# R15 · 启动报错归因（2026-09-03，公开阅读版）

- 记录范围：来源轮次 `R15-2026-09-03-boot报错归因分析`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R15-2026-09-03-boot报错归因分析/narrative.md`，内容 SHA-256：
  `2bd656dda8ebed3fd60d96404954c2c39b76f581676d0b5a52d8e818fe827a3e`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E4 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R15.md @ 5a3d784a541f`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R15/R15-originals.tar @ 7577cd43ff72`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮无 R5 冻结项；结论为 10 类 boot 报错全部 pre-existing、ours=0，
  不新增补丁、不修改实现（request.md#L59-L61）

## 一、解决什么问题

R14 暂停期间临时任务：用户重启时发现很多报错，要求分析这些报错是什么原因、是不是
本项目的问题；是我们的问题就修改去掉。报错清单（OCR，部分字符失真）：AMD-Vi 无
southbridge IOAPIC、microcode 不支持 CPU vendor、hwinfo 系列（dp/hdmi/vga skip →
default 输出 → DDCCI）、debugfs「Directory already present」等。

## 二、怎么干的

只读归因（无安装/无挂起/无热切）：复盘 6 个 boot journal（2026-09-02 11:50 至
2026-09-03 16:13，覆盖 patch-024 首次安装后重启至 4.0.2-i3 当前 boot）→ 把用户 OCR
的 8 类消息 + journal 补充的 SRSO 与 ACPI _DOD/_DOS 按可区分失败链拆为 10 类 → 逐类
在 6/6 boot 中逐字命中 → 对 patch-024/026/028/029 做路径排除 → 归因表与事故记录
文档化（不新增补丁）→ qoder 初审未回、用户指示收尾、dsh 直接终审。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R15-2026-09-03-boot报错归因分析/report.md#L5-L40 @ dff69f059df5`（归因结论/做了什么/验证）
- 本机来源（非 Git、公开检出不可取得）：`collab/R15-2026-09-03-boot报错归因分析/report.md#L63-L106 @ dff69f059df5`（dsh 终审与作者确认规则演进）
- 本机来源（非 Git、公开检出不可取得）：`collab/R15-2026-09-03-boot报错归因分析/request.md#L20-L73 @ 412ccf99e633`（dsh 提示词与定稿）
- 本机来源（非 Git、公开检出不可取得）：`collab/R15-2026-09-03-boot报错归因分析/qoder-notes.md#L27-L97 @ eb894e52026b`（qoder 归因预判表与方法补充）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R15/R15-originals.tar @ 7577cd43ff72`（原文快照）
- [boot-errors-attribution-20260903.md](../../incidents/boot-errors-attribution-20260903.md)；历史版本锚：`docs/incidents/boot-errors-attribution-20260903.md#L1 @ 0f9b4e1f6586`
- 事实值（非指针）：10/10 pre-existing、ours=0；交付时 HEAD==origin/main（e2cd006）；
  提交 0f9b4e1；门禁 PASS/BLOCKED/CLEARED/BLOCKED、PASS_DOCS、license 50/50、
  collab 26/26、suspend 60/60

## 三、发现了什么问题（错误资产）

1. OCR 校正与遗漏项补入（E1）：用户 OCR 写的是 drmengine，实际 journal 记录为
   dmaengine——根因是厂商驱动两个 DMA 引擎驱动（inno_axi_dma_drv.c 与
   inno_pcie_dma_drv.c）都绑定 PCI 设备 0000:02:00.0，dmaengine debugfs 目录二次
   注册触发警告（qoder-notes.md#L42-L64）；用户 OCR 遗漏的 Speculative Return Stack
   Overflow 与 ACPI _DOD/_DOS 因 6/6 journal 均存在，按 dsh 定稿要求补入独立归因项
   （report.md#L14-L22、#L44-L47）。处置：归因表注明校正与补充。教训：OCR 输入必须
   回原文校正后再归因；归因表要覆盖 journal 全部报错类而非只回应用户清单。
2. allowlist 收尾缺口（E2）：dsh 终审独立核验发现新文档未入 allowlist（审计 FAIL
   allowlist_incomplete）——已重生成 allowlist 并复核：审计 PASS/BLOCKED/CLEARED/
   BLOCKED、PASS_DOCS、license 50/50、collab 26/26、suspend 60/60、diff check 干净
   （report.md#L68-L70）。教训：新增文档必须同步 allowlist，审计 FAIL 是真实门禁
   信号而不是可跳过的噪声。
3. 提交未经作者确认的流程缺陷（E3）：R15 已按收尾指示提交 0f9b4e1，但提交前未获
   作者（codex）确认——按用户新规则「是谁写的，就让谁确认是否要被提交」事后补确认。
   规则历经三次修订定稿（report.md#L76-L106）：「谁写的谁决定提交。dsh 保留意见权
   与 git 执行/门禁复核，不替代作者决定；用户最终拍板可推翻」（第一版）、「谁写的
   文件，谁申请提交；最终由 dsh 拍板」（澄清版）、最终定稿「谁写的谁决定提交；dsh
   是最终评审，发现问题可要求作者修改」。0f9b4e1 记录为「用户指示代行」特例。
   教训：提交授权链必须闭合——作者确认、dsh 评审、用户拍板，任何一环缺失都要显式
   记录为特例。
4. 归因结论（E4）：6 个 boot journal 覆盖 2026-09-02 11:50 至 2026-09-03 16:13，全部
   10 类均逐字出现在 6/6 boot；结论为 pre-existing，ours=0（report.md#L7-L10）。
   dsh 独立抽验：「R12（pre-029）与 R13（post-029）证据 journal 中 "already present"
   各 1 次、hwinfo/DDCCI 系列分别 45/53 次——10 类报错均 pre-existing，024/026/028/029
   未引入」（request.md#L59-L61）。
   处置：按 pre-existing 文档化并列上游/厂商待办，不新增补丁。教训：非本项目引入不等于无害，归因和处理优先级须分开。

## 四、怎么解决的

dsh 定稿（request.md#L57-L73）：不新增补丁、不修改实现；文档化任务——新增
docs/incidents/boot-errors-attribution-20260903.md（归因表 + 证据 boot 时间线 +
处理建议）并登记 incidents/README；code-analysis.md / current-work.md 引用已知
pre-existing 清单（AMD-Vi BIOS quirk、microcode vendor 消息、hwinfo_g0m.bin 缺失 →
DDCCI 降级（背光后果已由 029 处理；dp/hdmi/vga default 输出为厂商降级路径）、
dmaengine debugfs 目录冲突）；current-work 的上游/厂商修复报告待办补充具体项。
dsh 终审（report.md#L63-L71）：通过，批准提交（用户指示收尾）；修复 allowlist 收尾
缺口后复核全绿，按 §七 批准提交。

## 五、特别说明

- 冻结措辞纪律：「pre-existing」不等于「无害」：hwinfo_g0m.bin 缺失仍是已知厂商
  固件边界（其背光后果已由 patch-029 处理，亮度控制仍缺）；dmaengine 双引擎同设备
  名冲突仍需厂商侧处理；本轮不将其改写为 DRM 问题。
- 历史命令不构成执行授权：本轮没有修改任何实现路径，也没有尝试消除告警；若后续
  获得 hwinfo 固件候选，安装与重启验证另行批准。
- 边界纪律：不修改 drivers/、baselines/、patches/；文档-only 变更；R14 维持暂停，
  R15 结案后由用户决定恢复 R14。
- 后续影响：作者确认规则入规约 §九（提交决定权归作者、dsh 最终评审）；「用户指示
  代行」特例记录在案。
