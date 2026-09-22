# R05 · 真机 A/B 验收（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R05-2026-09-02-suspend-resume真机A-B验收`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R05-2026-09-02-suspend-resume真机A-B验收/narrative.md`，内容 SHA-256：
  `c16700287fad4813a1110154bf6773c69a087ff524a893dfae338fbd8d69aa02`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 保留已有事件的事实、处置与教训，不另改写结论；移除末尾私有抽验表，公开正文可独立阅读。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。
6. R05 是历史轮次编号，与后来的 R5 测试项不同；来源没有后者冻结词，本页另补当前 R5=FAIL 与 OUTSIDE_COVERAGE，不反写到历史验收中。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R05.md @ 1de38fa9bc70`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R05/R05-originals.tar @ c13aef4ae765`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

本轮结论口径——「i2 单次 s2idle PASS，支持 patch-025 假设；因果 UNVERIFIED」
  （report.md#L175）；deep 仍未执行、维持未验证

## 一、解决什么问题

执行 R04 终审提出的重启式受控 A/B 验收：先免重启 pre-flight（回退包复核、i2 兼容性
预检、取证通道），再重启安装 4.0.1-i2 做非挂起显示基线，然后 s2idle A/B（i2 先测、
硬门禁=人工可见画面），deep 仅在阶段 2 全绿且用户本人在场同意时执行。目标是为
patch-025 的因果提供同条件对照证据。

## 二、怎么干的

阶段 0 pre-flight：三份回退包 SHA 一致（68aea6c0…8735）、i2 离线 DKMS/vermagic PASS、
与 4.0.0 的符号/depends/__versions CRC 全量比较 PASS（12,839 定义符号、768 导入符号）。
阶段 1：完整 deb 安装 + 有序重启（不热切），PASS_INSTALL_STATUS，人工内屏/外屏/TTY
基线硬门禁 PASS。阶段 2：布设 300 秒一次性 watchdog + RTC 60 秒唤醒，i2 s2idle 全
门禁 PASS（原红屏未复现）；用户评估风险后明确决定不做 4.0.0 同条件对照。阶段 3 deep
未执行。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R05-2026-09-02-suspend-resume真机A-B验收/report.md#L5-L78 @ c221fe6646b6`（阶段 0/1/2/3 全程）
- 本机来源（非 Git、公开检出不可取得）：`collab/R05-2026-09-02-suspend-resume真机A-B验收/report.md#L122-L152 @ c221fe6646b6`（R06 严格定位方案）
- 本机来源（非 Git、公开检出不可取得）：`collab/R05-2026-09-02-suspend-resume真机A-B验收/report.md#L168-L194 @ c221fe6646b6`（dsh 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R05-2026-09-02-suspend-resume真机A-B验收/request.md#L10-L63 @ 7a43ef3ced59`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R05/R05-originals.tar @ c13aef4ae765`（原文快照）
- 事实值（非指针）：HEAD=origin/main=3aafaa93634b；本轮无 tracked 变更（staged/
  unstaged/untracked 均为 0）；4.0.0 同条件对照 NOT RUN、deep NOT RUN

## 三、发现了什么问题（错误资产）

1. 完整 A/B 未完成（E1）：i2 通过后用户决定不承担 4.0.0 再次红屏、失联、强制重启及
   两次包切换风险；不违反「如用户同意」条件，但使因果结论保持未证实——本轮结论限定
   为「i2 单次 s2idle 完整验收通过，支持 patch-025 假设，但没有同条件 A/B，不证明
   因果」（report.md#L71-L73、#L104-L106）。教训：对照试验的风险由用户拍板，结论口径
   必须随证据强度收缩。
2. deep NOT RUN（E2）：完整阶段 2 A/B 未结束，且用户选择本轮停止风险试验，不满足
   阶段 3 双重前置条件（report.md#L75-L78、#L107-L108）。教训：前置条件不满足即不
   执行，deep 保持未验证。
3. 证据链小事故（E3）：第一份非 root pre-kernel journal 只写入了权限提示；唤醒后用
   宿主 root 权限保存的完整当前 boot journal 同时覆盖挂起前与唤醒后窗口，证据链未
   丢失（report.md#L109-L110）。教训：关键证据必须以授权权限落盘。
4. cursor 直接观测不可得（E4）：crtc-*/cur_img 的 debugfs 读取返回 Operation not
   permitted/Bad address，无法直接记录光标图像；保留该负证据，未将 atomic/framebuffer
   间接证据冒充 cursor 寄存器直接证据（report.md#L59-L60、#L111-L112）。教训：观测
   边界如实记录，不伪造证据。
5. 入组条件差异（E5）：挂起前外屏处于无 EDID 临时 1920x1200，唤醒后重探测为真实
   EDID 1920x1080，对应 framebuffer/FB ID 更换、connector link-status Good——这是实际
   modeset/reprobe，不冒充「状态逐字不变」（report.md#L68-L70）。教训：A/B 入组必须
   固定 EDID/mode 等外部变量（成为 R06 方案第 1 条）。

## 四、怎么解决的

dsh 监督裁决（report.md#L168-L177）：「R05 终审：通过……结论口径维持诚实：i2 单次
s2idle PASS，支持 patch-025 假设；因果 UNVERIFIED。设备当前运行 4.0.1-i2、mem_sleep=
s2idle，任何新异常立即按已核对回退包回退。R05 无 tracked 变更，无需提交；INDEX 状态
置为通过。」随后 dsh 批准 R06 严格定位方案（最小 A→B→A→B、单变量、ftrace 调用计数、
固定 EDID/1920x1080/唤醒方式、4 次安装重启 + 4 次 s2idle、预算 4–6 次重启、不做 deep），
并附加 5 项执行条件（report.md#L179-L194）。

## 五、特别说明

- 冻结措辞纪律：patch-025 因果仍为 UNVERIFIED；不得以「已通过/已证明」措辞覆盖；
  deep 未验证、不得使用（日常仅 s2idle）。
- 历史命令不构成执行授权：报告中的回退命令（sudo apt-get install --reinstall
  --allow-downgrades ./build/innogpu-fh2m-trixie_4.0.0-i1.deb；sudo systemctl reboot）
  为历史记录，不构成执行授权。
- 边界纪律：drivers/、baselines/、binary-manifest.json、监督分支零改动；不热切模块；
  不改 dotfiles；license/1C 不变。
- 后续影响：R06 严格定位需用户在场执行；任何 RTC 唤醒失败、SSH/TTY 不恢复或 watchdog
  异常立即终止整轮。
