# R13 · 面板与背光恢复（2026-09-03，公开阅读版）

- 记录范围：来源轮次 `R13-2026-09-03-suspend-resume显示恢复背光专项`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/narrative.md`，内容 SHA-256：
  `d0029e80d18e9e24dfc2669d55f63f564f382b891f923d547352497310e2b439`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E5/E6/E7 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R13.md @ 231b8538be6d`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R13/R13-originals.tar @ 76e916747a81`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

正式验收仍需 R14（3 次连续 deep + 供电/外屏组合），单次 deep 不构成
  完整验收（report.md#L553-L557）

## 一、解决什么问题

R12 共识：patch-028 达成 PVR 修复目标，但 4.0.2-i2 整体 deep 验收 FAIL——故障域收窄
到面板输出/背光/DPU 硬件输出链路（DPU 扫描正常 + eDP 链路正常 + X 已绘制，唯独内屏
黑屏）。本轮为显示恢复专项：阶段 1 复盘 + 只读观测设计（无挂起、无安装），经 A 线
（固件）→ B 线（patch-029）演进后实施最小修复并在阶段 2 做一次受控 deep 验证。

## 二、怎么干的

阶段 1：证据复盘（patch-025 对当前黑屏无效的否定性结论：cursor_enable=0 时
innodpu_pdp0_wakeup 为 no-op）→ 路径分析（R12 journal 记录 hwinfo 失败后回退 DDCCI；
DDCCI 枚举值 0 不在 panel 创建条件 PWM0..AUX_HDR(1-3) 内 → dp_dev->panel 可能为 NULL
→ resume 不执行 panel/backlight 使能——优先候选，非已证实根因）→ 符号可见性预检
（10 主目标精确可见 8 个，panel_set_pwr_state 需 .part.0 替代，backlight_pwm_set
unavailable）→ A 线固件定位（hwinfo_g0m.bin 本地与已验证公开来源均未提供）→ B 线
patch-029 最小 v1 diff（panel 创建条件下界放宽到 DDCCI + inno_panel_backlight_init()
对 DDCCI 显式 early-return 0；不伪造 brightness sysfs；connector->force 维持
PWM0..AUX_HDR 范围）→ 4.0.2-i3 = 024+026+028+029、epoch 1788796800 → 阶段 2：安装
重启 + 一次受控 deep（RTC 90 秒、无自动 reboot watchdog、SSH 取证）。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L5-L100 @ 578c1e3bddf4`（codex 方案反馈）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L103-L178 @ 578c1e3bddf4`（阶段 1 做了什么/验证/遗留）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L205-L224 @ 578c1e3bddf4`（dsh 终审：DDCCI 假设独立复核成立）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L264-L360 @ 578c1e3bddf4`（codex B 线反馈与 patch-029 草案）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L373-L400 @ 578c1e3bddf4`（阶段 1 实现与验证）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L443-L472 @ 578c1e3bddf4`（返工与返工通过）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/report.md#L488-L557 @ 578c1e3bddf4`（阶段 2 执行与终审）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/request.md#L49-L180 @ fc340af9c2db`（dsh 裁定补充/终裁/A 线/B 线定稿）
- 本机来源（非 Git、公开检出不可取得）：`collab/R13-2026-09-03-suspend-resume显示恢复背光专项/qoder-notes.md#L206-L346 @ 11082a46dacc`（qoder B 线方案建议）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R13/R13-originals.tar @ 76e916747a81`（原文快照）
- [029-suspend-resume-ddcci-panel.patch](../../../patches/029-suspend-resume-ddcci-panel.patch)；历史版本锚：`patches/029-suspend-resume-ddcci-panel.patch#L1 @ e2cd0061d6df`
- [029-suspend-resume-ddcci-panel.md](../../patches/029-suspend-resume-ddcci-panel.md)；历史版本锚：`docs/patches/029-suspend-resume-ddcci-panel.md#L1 @ e2cd0061d6df`
- 事实值（非指针）：4.0.2-i3 deb SHA 177133ee…9e1662；epoch 1788796800（2026-09-08
  00:00 +0800）；suspend 60/60、version 12/12；提交 e2cd006；阶段 2 无 tracked 变更

## 三、发现了什么问题（错误资产）

1. dsh 终审返工 3 项（E1）：patch-029 静态验证阶段首次终审返工——P1：
   license-audit-policy.json 缺 029 登记（补 "patches/029-suspend-resume-ddcci-
   panel.patch": "MIT OR GPL-2.0-only"）；P2（qoder 1）：report.md 顶部状态行仍写
   「只读复盘与观测设计」，应反映 B 线 patch-029 实现完成；P2（qoder 2）：report 中
   version 计数写 11/11，实际 12/12（i3 排序用例）；附加：9 个文件未暂存，须 git
   add -A 全量暂存后重跑全门禁（report.md#L443-L459）。返工通过（report.md#L464-
   L472）：「返工 4 项全部核验：policy 029 登记、report 状态行与 version 12/12 修正、
   23 文件全量暂存（2 新 21 改、无 unstaged/untracked）、保护区零改动。」教训：报告
   状态行与计数必须随实现进展同步更新；policy 登记是仓库惯例不是可选项。
2. kprobe 事件名点号 EINVAL（E2）：首次 R13 observer 注册因 kprobe 事件名直接包含
   panel_set_pwr_state.part.0 的点号而返回 EINVAL，发生在合盖和 /sys/power/state
   写入之前；该窗口已清理并保留失败证据。随后使用合法事件别名
   panel_set_pwr_state_part_0 重做 prepare，5 秒只读 preflight 通过
   （report.md#L504-L507）。教训：kprobe 事件名不得含点号，符号变体须预检后映射
   别名。
3. 「9 个探针」计数笔误（E3）：dsh 补充称「扩展 9 个探针」，清单实际含 10 个符号；
   codex 方案反馈指出冲突并按 10 个目标设计；dsh 终裁「主清单按 10 个符号保留（codex
   处理正确），全文案统一为"10 个主清单符号"，不静默删减」（report.md#L22-L26、
   #L197-L200）。教训：清单计数必须与逐项枚举核对，不得静默删减。
4. A 线固件未果（E4）：hwinfo_g0m.bin 定位调查结论为「本地与已验证公开来源均未提供」
   （非绝对不存在）；本机 /lib/firmware/innogpu/ 仅有 fh2c/fh2m 四个文件，无
   hwinfo_g0m.bin（request.md#L127-L129；report.md#L209-L214）。处置：保留用户侧
   渠道（原系统镜像/OEM/Deepin 内部包），未来若提供候选 A 线优先复活；本轮转 B 线。
   教训：固件缺口结论要区分「未找到可验证来源」与「绝对不存在」。
5. 阶段 2 单次 deep display PASS（E5）：安装 4.0.2-i3 重启后启动日志确认
   innogpu/hwinfo_g0m.bin 仍为 -2 ENOENT、随后 DP 路径记录 Use default DDCCI——patch-029
   的 DDCCI panel 路径实际被触发；deep（16:49:05 入口、16:50:40 RTC 返回）恢复 trace
   含 pvr_pm_resume、PVRSRVDeviceResume、ResumeDVFS、hal_power_wakeup、
   drm_atomic_helper_resume、pdp0_crtc_atomic_enable、inno_dp_encoder_mode_enable、
   inno_panel_prepare、panel_set_pwr_state.part.0 和 inno_panel_enable；backlight_pwm_set
   不可见记 unavailable；/sys/class/backlight 保持 UNAVAILABLE reason=
   no_backlight_device；PVR 八项均为 0；用户人工确认内屏画面正常、键盘/鼠标/TTY
   正常（report.md#L490-L523）。dsh 终审（report.md#L543-L557）：「结论：patch-029
   在无 hwinfo 固件的 DDCCI 回退条件下，单次受控 deep 唤醒 display PASS（R12 同条件
   FAIL → PASS）；修复机制获真机证据支持；无伪造 backlight，诚实边界保持。4 个 P3
   观察项（prepare ×18 内部结构、backlight_pwm_set unavailable 属设计预期、ack 时
   外屏已重接、单次 deep）记录在案，不阻塞。」注意口径：单次 deep，正式验收仍需
   R14 矩阵。
   教训：单次机制支持与正式矩阵验收分开，不能跨过后续验收门槛。

6. tracefs 未挂载（E6）：阶段 1 未执行实际 kprobe/ftrace 采样，因为当前 tracefs 未
   挂载；这是可追踪性预检结果，不是失败。阶段 2 前必须重新建立 tracefs 后再决定
   目标项可用性（report.md#L154-L156）。
   教训：接口预检不可用不能冒充探针已运行，也不能推成目标永远不可观测。

7. 待验证假设纪律（E7）：qoder B 线方案建议中 boot 时序 GPIO HIGH→LOW 表格被 codex
   指出为「待验证假设」——panel_pwr_gpio_init() 的「default gpio high」是 callback
   执行时写入的配置，不是 BIOS 启动状态的观测；inno_dp_hw_fini() 实际 shipped 实现
   是否复位 REG_M_BL_GPIO 不能从当前可读源码推出；dsh 定稿接受该收紧，patch-029
   文档因此标注为「候选修复」而非「已验证修复」（report.md#L290-L302）。
   教训：配置默认值、回调写入和真实启动电平是不同证据。

## 四、怎么解决的

最小 v1 diff：innodpu_dp_debugfs.c panel 创建条件下界由 CONNECTOR_BACKLIGHT_PWM0
放宽到 CONNECTOR_BACKLIGHT_DDCCI；connector->force = DRM_FORCE_ON 维持原
PWM0..AUX_HDR 范围（与 GPIO 恢复因果分离）；innodpu_panel_backlight.c 对 DDCCI 显式
early-return 0（有意设计：无 backlight device，不依赖 -EINVAL 错误路径）。dsh 终审
返工通过后批准提交（report.md#L464-L472）；阶段 2 用户批准执行并结案（report.md#L543-
L557）。已知能力缺口保持诚实：DDCCI brightness device 不注册（缺少可验证的 FH2M
DDC/CI 控制契约），不伪造 brightness sysfs、不把 DDCCI 静默改成 PWM
（report.md#L417-L428）。

## 五、特别说明

- 冻结措辞纪律：单次受控 deep display PASS 不得外推为正式验收结论；R14（3 次连续
  deep + 供电/外屏组合）全过后 P3 才可建议「已修复待 dotfiles 复核」；patch-025
  仍 UNVERIFIED。
- 历史命令不构成执行授权：报告中的安装、重启、deep 挂起命令均为历史记录；未执行
  回退安装、再次重启或第二次 deep；后续 hwinfo 固件安装与重启需用户另行批准。
- 边界纪律：修复以补丁表达、drivers/ 工作树零改动；不热切模块；不改 dotfiles；
  license/1C 不变；阶段 2 无 tracked 变更。
- 后续影响：4 个 P3 观察项记录在案；A 线固件渠道保留；brightness sysfs 属独立功能
  项，须另立功能项并验证 VCP 读写、映射、错误处理与 resume 行为。
