# R06 · 严格定位 A/B（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R06-2026-09-02-suspend-resume严格定位A-B`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R06-2026-09-02-suspend-resume严格定位A-B/narrative.md`，内容 SHA-256：
  `f64f79ff372ea31a4cb6fb49fdee26cc132fcb4548ac1c990833aa1611766547`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 在 E4/E5 补明处置、教训或验证边界；原事件文本保留，不复制核对表列、验证脚本或审查结果。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R06.md @ 96c7ee77987b`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R06/R06-originals.tar @ 1b012fa0c903`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

patch-025 与红屏之间仍为 UNVERIFIED；deep 继续冻结（report.md#L339）

## 一、解决什么问题

按 R05 报告已批准的严格定位方案执行最小 A→B→A→B（A=patch-024 保留 post-atomic
wakeup；B=patch-024+025），用 ftrace 调用计数 + 固定外部变量证明 patch-025 是否为
红屏消失的原因；并尽量向寄存器级机制层深挖，给出 deep 路径影响与更原则性修复候选。

## 二、怎么干的

预检（kallsyms 四函数可见 + tracefs 可写）→ 首轮前阻断：包级单变量失败（B 多带
.orig）→ dsh 批准准备子阶段（025 重生为 -U3 单 hunk 纯删除；构建器 --fuzz=0
--no-backup-if-mismatch 与 .orig/.rej 三级拒绝；包边界反例 +2；重建 A=4.0.1-i3、
B=4.0.1-i4，同 epoch 1788451200；i1/i2 不再复用）→ 提交 b499b8a → 真机第 1/4 轮
A=i3 一次 s2idle：无红屏、cursor 分支未执行 → 命中预定停止条件「A 不稳定红屏即停」，
1/4 后终止整轮 → 完成寄存器级机制、deep 影响与原则性修复分析。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/report.md#L19-L32 @ 342f6fede00a`（.orig 阻断发现）
- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/report.md#L114-L180 @ 342f6fede00a`（准备子阶段执行报告）
- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/report.md#L203-L261 @ 342f6fede00a`（真机执行记录与停止）
- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/report.md#L263-L334 @ 342f6fede00a`（最终因果结论与机制分析）
- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/report.md#L371-L398 @ 342f6fede00a`（dsh 裁决与用户决策）
- 本机来源（非 Git、公开检出不可取得）：`collab/R06-2026-09-02-suspend-resume严格定位A-B/request.md#L8-L64 @ 966bf9a9ecde`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R06/R06-originals.tar @ 1b012fa0c903`（原文快照）
- [025-suspend-resume-display.patch](../../../patches/025-suspend-resume-display.patch)；历史版本锚：`patches/025-suspend-resume-display.patch#L1 @ b499b8ac7cc4`（-U3 重生版）
- [build-innogpu-driver.sh](../../../scripts/build-innogpu-driver.sh)；历史版本锚：`scripts/build-innogpu-driver.sh#L1 @ b499b8ac7cc4`（strict patch 三级拒绝）
- [check-release-package.sh](../../../scripts/check-release-package.sh)；历史版本锚：`scripts/check-release-package.sh#L1 @ b499b8ac7cc4`（.orig/.rej 反例）
- [.gitattributes](../../../.gitattributes)；历史版本锚：`.gitattributes#L1 @ b499b8ac7cc4`
- 事实值（非指针）：A=i3 SHA 6cab9e52…9dcba；B=i4 SHA 085e0684…969a7；epoch
  1788451200（2026-09-04 00:00 +0800）；准备阶段全量 413/413；真机 1/4 轮
  atomic_resume=1 / crtc_atomic_enable=1 / post_wakeup=3 / cursor_resume=0

## 三、发现了什么问题（错误资产）

1. 包级单变量被 .orig 破坏（E1）：/tmp 解包对比发现 B 多一个
   usr/src/innogpu-kernel-2.2/innosrvkm/innodpu_drm_pm.c.orig——patch-025 为零上下文
   补丁，第二个 hunk 因前一个 hunk 删行产生 offset，GNU patch 按「mismatch 时备份」
   默认行为生成 .orig；R04 的边界检查只拒绝 .o.cmd，未拒绝 .orig（report.md#L19-L32）。
   处置：第 1/4 轮安装前停止，请 dsh/用户评审；dsh 批准准备子阶段（dsh 裁决
   report.md#L87-L110：补丁重生带 3 行上下文、构建器硬门禁、包边界新增反例、包级单
   变量复验、同 epoch 新版本 i3/i4）。教训：包级对照必须以完整 payload 双向 diff 为准，
   不能只比较目标 .c 或 .ko；零上下文补丁不再作为交付形态。
2. kprobe 烟测首败（E2）：一次性脚本错误地以 > 写 kprobe_events，tracefs 先清表后再
   处理删除命令，导致删除返回 ENOENT；初次运行不能作为通过证据（report.md#L220-L222）。
   处置：脚本改为仅使用追加接口 >>，并在变更前持久化事件表、current_tracer 与
   tracing_on 快照；复验初始表为空、唯一事件 r06/r06_smoke_996975 注册/删除成功，三项
   状态逐字一致（report.md#L223-L229）。教训：tracefs 接口必须先快照再变更，删除类
   操作不能用覆盖式写入。
3. 预定停止条件命中（E3）：A 的第一轮没有复现红屏，且候选 pdp0_cursor_resume 分支
   没有执行（wakeup=3、cursor_resume=0）；按「A 若不稳定红屏立即停止」规则在 1/4 后
   终止：不安装 i4、不做 deep、不改 dotfiles（report.md#L256-L261）。因果结论：
   patch-025 维持 UNVERIFIED；A 正常恢复削弱「只要保留 post-atomic hook 就必然红屏」
   的强假设，条件假设仍与证据相容（report.md#L265-L271）。教训：A/B 必须同时满足
   包级单变量和运行时 treatment activation；不能靠增加盲测轮次代替触发条件控制。
4. 机制分析（E4）：pdp0_cursor_resume() 写寄存器 ID 0x259/0x25a/0x258 与 0x0c000000
   控制位后经 set_config_valid 提交；innodpu_pdp0_backup() 在 shipped object 中是空
   函数，当前代码没有形成对称的 suspend 快照/恢复代次——post hook 无法判断 atomic
   路径是否已经提交过相同状态（report.md#L283-L293）；deep 不得按 mem_sleep 标签分支、
   纯删除 025 不得在 deep 验证前晋级（report.md#L314-L326）。
   处置：将状态代次与 pending 标志并入后续候选方向，deep 仍不放行。教训：恢复协议要有对称快照和提交归属，不能仅凭重复调用猜测安全。

5. 用户决策三项（E5）：接受停止结论；创建 R07 观测轮次；保留 4.0.1-i3（不回退），
   风险提示已告知：i3 与 R03 红屏组合同构；日常保持 mem_sleep=[s2idle]，deep 未验证、
   不得使用（report.md#L392-L398）。
   教训：用户接受保留候选的风险不等于候选获得稳定性背书。

## 四、怎么解决的

dsh 监督裁决（report.md#L371-L388）：「结论定级认可：patch-025 维持 UNVERIFIED；
"无条件必红屏"的强假设被削弱，条件假设（cursor_enable/特定 DPU 状态）仍与证据相容；
次优假设四条排序合理。机制分析认可：0x258..0x25a + config-valid 影子提交机制假设、
backup() 空实现导致的非对称恢复、deep 不得按 mem_sleep 标签分支、纯删除 025 不得在
deep 验证前晋级——均为正确工程判断；原则性修复方向（cursor 状态代次 + pending 标志
+ 并入 atomic plane）作为后续候选。」准备阶段提交 b499b8a；真机阶段无 tracked 变更，
Git 保持 HEAD=origin/main=b499b8a、工作区干净。

## 五、特别说明

- 冻结措辞纪律：patch-025 维持 UNVERIFIED，不能声称修复有效、也不能声称已排除该
  假设；当前机器运行 4.0.1-i3 只完成一次无红屏 s2idle，不是稳定发布结论。
- 历史命令不构成执行授权：报告中的安装/重启/挂起/回退命令均为历史记录，未获新批准
  不得执行；未经新批准不再安装、重启或挂起。
- 边界纪律：drivers/、baselines/、binary-manifest.json、监督源码树零改动；不做 deep、
  不修改 dotfiles；tracked 变更仅准备阶段（已提交 b499b8a）。
- 后续影响：下一步建议先做无行为变化的 observability 候选（记录每 DPU 的 active、
  cursor_enable、cursor_fb/x/y/w/h、目标函数指针与寄存器读回），只有在 A 挂起前明确
  证明 cursor 分支已入组且 A 可重复红屏后才恢复 i3/i4 对照（report.md#L328-L334）。
