# R09 · 单次 s2idle 复现（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R09-2026-09-02-suspend-resume单次s2idle复现`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R09-2026-09-02-suspend-resume单次s2idle复现/narrative.md`，内容 SHA-256：
  `1d1816a82292fe91e0bbd39792e5ed2bd48c76e4d460433972788739891643ab`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 保留已有事件的事实、处置与教训，不另改写结论；移除末尾私有抽验表，公开正文可独立阅读。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R09.md @ aa176495e21a`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R09/R09-originals.tar @ 884fd0b360bc`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

patch-024 deep 修复与 patch-025 均保持 UNVERIFIED，deep 继续冻结，P3 保持
  打开（report.md#L147-L151）

## 一、解决什么问题

在 4.0.1-i3 上执行一次（仅一次）带完整观测器的 s2idle 挂起，尝试复现红屏并采集
primary FB/GEM、DPU shadow/config-valid、connector、PVR 与四函数 trace 证据；无论
复现与否只做这一次，不追加盲测。

## 二、怎么干的

Pre-flight：确认内核 6.12.101+deb13-amd64、包 4.0.1-i3、mem_sleep=[s2idle] deep；
三份 4.0.0-i1 回退包 SHA 逐一复核；固定本轮显示基线（仅内屏 eDP-1、2560x1600@90、
EDID SHA 0e71a3b2…5c28a）；15 秒 pre-flight abi_gate=PASS、27 probes attach；布设 60 秒
RTC 与 300 秒一次性 watchdog（用户「确认挂起」后才布设）；单次 s2idle 22:21:42→
22:22:46 后保存 post 同组证据与人工判据；ack 通过，未执行第二次挂起。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R09-2026-09-02-suspend-resume单次s2idle复现/report.md#L7-L64 @ 9436b57fff5b`（pre-flight/单次 s2idle/恢复路径观测/假设影响）
- 本机来源（非 Git、公开检出不可取得）：`collab/R09-2026-09-02-suspend-resume单次s2idle复现/report.md#L131-L151 @ 9436b57fff5b`（dsh 裁决与用户决策）
- 本机来源（非 Git、公开检出不可取得）：`collab/R09-2026-09-02-suspend-resume单次s2idle复现/request.md#L7-L44 @ 3b7e6c8b8698`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R09/R09-originals.tar @ 884fd0b360bc`（原文快照）
- [current-work.md](../../state/current-work.md)；历史版本锚：`docs/planning/current-work.md#L1 @ fbb1f003c603`（暂停 suspend 线决策入档）
- 事实值（非指针）：suspend_start=2026-09-02T22:21:42+08:00、suspend_return=
  2026-09-02T22:22:46+08:00；PVR 八项计数 pre/post 全 0（unchanged=8）；三缓冲设备
  地址 0x603dfac000/0x60530ed000/0x605d5f3000；HEAD=origin/main=28729b3

## 三、发现了什么问题（错误资产）

1. SSH 未测试如实记录（E1）：当前对话通道不能冒充外部 SSH 验证，因此如实记为未测试；
   内屏和 TTY 已由用户确认，足以在恢复后撤销 watchdog，但不把 SSH 写成 PASS
   （report.md#L94-L96）。教训：判据通道缺失时如实标注 NOT_TESTED，不以本地状态
   冒充外部验证。
2. 旧控制脚本误用（E2）：首次异步 prepare 使用了修改前已加载的旧控制脚本，虽得到
   observer PASS，但缺少新增的 PVR counter 文件。该状态经脚本 abort 清除指针并保留
   审计目录，随后以修正版重新 prepare；只有 20260902-221908 目录作为本轮权威证据
   （report.md#L96-L98）。教训：脚本版本必须随 prepare 结果绑定，旧版本产物不得
   作为权威证据。
3. dmenupass 噪音（E3）：dmenupass 在 sudo 命令完成时多次打印 echo: I/O error，但
   sudo 后的控制命令均有独立 PASS 和落盘证据；该 UI 收尾警告不影响实验结果
   （report.md#L99-L100）。教训：UI 层噪音与结果证据分离判断。
4. 证据目录权限（E4）：控制脚本归还 evidence 所有权后，父目录仍为 root 0700，导致
   当前用户无法遍历；收尾时将本轮创建的 /var/tmp/innogpu-r09 设为 0711（不可列目录），
   具体 evidence 目录仍为用户 0700；该修正只影响本机证据可读性，不影响驱动或实验
   行为（report.md#L101-L103）。教训：证据目录属主/权限须在收尾阶段一并核对。

## 四、怎么解决的

单次结论：一次 s2idle 未复现红屏，不是稳定性或修复有效性证明（report.md#L36-L37）。
对假设的影响（report.md#L54-L64）：cursor/config-valid 假设未入组，既未证实也未排除；
primary FB/GEM 强假设被进一步削弱（恢复窗口完整地址链一致且画面正常），间歇性内容/
cache 竞态不能排除；HDMI 假设本轮无判别力；shadow/config-valid 强版本进一步削弱，
特定竞态或 inactive DPU 共享 bank 条件仍不能排除。dsh 关键限制（report.md#L138-L141）：
「本轮 HDMI 未连接，对 R03 外屏红屏对应的 HDMI link/color 假设无判别力；且原 P3 根
症状是 deep 唤醒（黑屏+SSH 死+PowerLock），patch-024 的 deep 修复自始至终未在 deep 上
验证。两次红线（deep 主缺陷、s2idle 外屏红屏）均保持未验证状态，P3 不可关闭。」用户
决策：暂停 suspend 线（推荐项），保持 i3 + s2idle 现状；恢复调查需重新授权
（report.md#L147-L151）。

## 五、特别说明

- 冻结措辞纪律：单次未复现 ≠ 修复；不得以「已通过/已修复」覆盖 patch-024/025 的
  UNVERIFIED 状态与 deep 冻结；P3 保持打开。
- 历史命令不构成执行授权：R09 已用完唯一一次挂起授权，不应追加第二次盲测；报告中的
  挂起/回退命令均为历史记录。
- 边界纪律：本轮未安装、未重启、未回退、未热切模块、未 modeset、未改 dotfiles；
  tracked 零改动；license/1C 不变。
- 后续影响：若优先追查 R03 故障，应在固定外屏、固定 mode 且增加安全 HDMI 状态观测后
  另行授权一次故障窗口采样，不应把本轮内屏成功外推为外屏已修复。
