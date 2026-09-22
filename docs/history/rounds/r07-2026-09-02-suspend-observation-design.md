# R07 · 观测与触发设计（2026-09-02，公开阅读版）

- 记录范围：来源轮次 `R07-2026-09-02-suspend-resume观测与触发设计`；下文历史“当前/本轮”指当时阶段。
- 阅读版整理：2026-09-22，R21；原始记录、历史裁定与 R17 的复算声明各保留其时点。
- **原文/快照仍本机（collab/ 与 .runtime-archive/，不入 Git）**。
- 来源：`collab/R07-2026-09-02-suspend-resume观测与触发设计/narrative.md`，内容 SHA-256：
  `3015d4ad1700efb23e1af08a7c95da52a242191572b74c7a37be5d512d70b3b0`。
- 当前结论以[status](../../project/status.md)为准；[轮次索引](../history.md#轮次公开阅读版索引)提供前后文。

脱敏与编辑差异清单（相对上述来源版本）：

1. 重设公开页头，移除本机 INDEX/待审状态和私有核对表结构；保留五段式与事件编号。
2. 原文/归档指针改为纯文本并标明不可随 Git 取得；tracked 指针补历史提交锚和当前有效导航。
3. 保留已有事件的事实、处置与教训，不另改写结论；移除末尾私有抽验表，公开正文可独立阅读。
4. 下文“本次实测/复算”等沿用来源的语句均指 **2026-09-20 R17 整理时点**，不是 R21 新跑历史测试。
   原录的中间产物失配、无法复算及权限限制仍保留；R21 核验范围另见本批交付报告。
5. 增加当前冻结边界；历史命令、历史放行、历史 PASS 均不构成当前执行授权。

来源辅助材料（全部**非 Git、公开检出不可取得**，不是附件下载链接）：

- `collab/R17-2026-09-19-文档优化迭代/error-asset-checklist-R07.md @ 62a1e4980246`（非 Git、公开检出不可取得；仅用于补齐事件，不发布审查表）。

行号读取约定：`report.md` / `request.md` / `qoder-notes.md` 及 r/q 缩写均指
下列快照内同名成员；其行号不是本公开页行号。S0 为文首来源叙事。
`.runtime-archive/r17-docs/archive-originals/R07/R07-originals.tar @ 88ae8fda3003`（非 Git、公开检出不可取得）。

当前冻结不变：**OUTSIDE_COVERAGE、R5=FAIL、禁止重跑、U1/U2、validation-results、未打 tag**；
U1/U2 未执行、validation-results 未签、签发与 tag 冻结。历史页不维护第二份运行现状。
正文中的 build/、debs/、/var/tmp/、collab/ 和 .runtime-archive/ 本机材料均为非 Git、
公开检出不可取得；历史设备路径与命令只用于解释证据，不能据此操作当前系统。

历史冻结与验收边界（来源原文，未改判）：

patch-025 长期保持 UNVERIFIED；deep 继续冻结（report.md#L86-L90）

## 一、解决什么问题

R06 结论：A=i3 第 1/4 轮 s2idle 无红屏、cursor_resume=0（候选分支未执行），025 维持
UNVERIFIED，次优假设四条已排序。用户三项决策：接受停止结论；创建 R07 无行为变化观测
轮次；保留 i3。本轮把 R06 机制分析入档，设计并实现只读观测器，先做非挂起采样判定
cursor 分支在常规桌面是否入组，并设计触发条件入组验证与挂起复现方案（只设计不执行）。

## 二、怎么干的

机制分析入档（tracked，全程 UNVERIFIED 口径）→ 新增 tools/probe-suspend-resume-
observer.bt（只附着自然发生的 cursor/config-valid/HAL 读写入口，不调驱动函数、不写
寄存器）与 tools/probe-suspend-resume-state.sh（固定内核/i3 包/厂商对象 SHA 与精确
DWARF ABI，绑定运行模块 build-id；输出只落入规范化 build/ 或 /tmp/ 新目录，拒绝既有
路径与符号链接）→ bpftrace --dry-run 13 探针 attach PASS → 5 秒非挂起采样：50 个状态
事件全部 cursor_enable=0、无 cursor 入口或 0x258..0x25a 自然访问 → 入组与后续实验
设计成文。

关键产物指针：

- 本机来源（非 Git、公开检出不可取得）：`collab/R07-2026-09-02-suspend-resume观测与触发设计/report.md#L5-L44 @ e1e6a5657b03`（入档/观测实现/采样/实验设计）
- 本机来源（非 Git、公开检出不可取得）：`collab/R07-2026-09-02-suspend-resume观测与触发设计/report.md#L107-L118 @ e1e6a5657b03`（dsh 裁决）
- 本机来源（非 Git、公开检出不可取得）：`collab/R07-2026-09-02-suspend-resume观测与触发设计/request.md#L7-L39 @ 4bb29dcbd160`（dsh 完整提示词）
- 本机来源（非 Git、公开检出不可取得）：`.runtime-archive/r17-docs/archive-originals/R07/R07-originals.tar @ 88ae8fda3003`（原文快照）
- [probe-suspend-resume-observer.bt](../../../tools/probe-suspend-resume-observer.bt)；历史版本锚：`tools/probe-suspend-resume-observer.bt#L1 @ 4d669a8dce52`
- [probe-suspend-resume-state.sh](../../../tools/probe-suspend-resume-state.sh)；历史版本锚：`tools/probe-suspend-resume-state.sh#L1 @ 4d669a8dce52`
- 事实值（非指针）：13 probes attach PASS；50 个 config-valid 事件全部
  cursor_enable=0；suspend fixture 24/24、license 50/50、collab 26/26、16 入口
  416/416；allowlist 222 个路径

## 三、发现了什么问题（错误资产）

1. BPF active 无法从 hwdev->crtc->state 取得（E1）：样本中 crtc 指针有效但 state 为空，
   因此 BPF active 明确标记 unavailable，以同时间窗 DRM debugfs 的 dpu1 active=1 为
   活动状态依据（report.md#L27-L28）；脚本现显式区分 active_valid，不再用 255 混淆
   （report.md#L75-L76）。教训：观测字段缺失时以独立通道补证并显式标注，不臆造。
   （该空 state 的根因在 R08 被确认为 shipped-object 偏移 1176 与运行模块偏移 1480
   的差异，R07 报告未改写。）
2. dmenupass 阻断加固后整包装器复跑（E2）：安全加固后曾发起 3 秒和 2 秒整包装器
   复跑，但 dmenupass 分别未提供密码或无法 grab keyboard，均在 sudo 执行前终止；没有
   探针启动，不作为证据。核心 BPF 程序在加固前已完成两次真实采样（report.md#L79-L82）。
   处置：dsh 附加条件——下次真机采样必须用加固后整包装器端到端跑一次并记录 PASS；
   包装器不得依赖图形 askpass——以 root/sudo -n 直接调用，askpass 不可用时在 sudo 前
   失败（report.md#L115-L117）。教训：真机采样工具链必须不依赖图形 askpass。
3. primary FB 内容 CRC 无安全接口（E3）：没有已确认的安全 primary scanout 内容 CRC
   接口；按要求报告 unavailable，未用 X 截图、fbdev hash 或主动 HAL 读取替代
   （report.md#L29-L30）。教训：证据缺口如实标记，不伪造替代证据。

## 四、怎么解决的

观测结论：当前 /etc/X11/xorg.conf 使用 modesetting DDX；内核 inno_dpu_cursor_set() 是
空实现，普通鼠标移动不进入厂商 cursor_set2/move，仅设置 SWCursor=false 不能作为入组
证据——常规桌面 cursor 分支天然不入组（report.md#L34-L38）。后续设计：若继续 cursor
假设须另开授权轮次（保存 Xorg 配置与回退命令后临时试验 vendor DDX 或可实际发起
cursor2 的路径；同时观测到 pdp0_cursor_set/move、cursor_enable=1 与目标寄存器自然写入
才算入组）；若始终不入组则转向 primary plane FB ID、pdp0_get_fb_dev_paddr 自然返回、
可用时 DRM CRTC CRC、DPU shadow/config-valid HAL 序列（report.md#L42-L44）。dsh 裁决
（report.md#L107-L118）：通过，批准提交；「未确认接口一律记 unavailable、不用截图/
主动 HAL 读取冒充，证据纪律正确」。

## 五、特别说明

- 冻结措辞纪律：patch-025 保持 UNVERIFIED，不得以「已修复/已通过」覆盖；i3 仅是
  当前实验候选，不是稳定版本；deep 冻结。
- 历史命令不构成执行授权：本轮未执行硬件光标入组或挂起；Xorg/DDX 切换、挂起实验等
  均须用户和 dsh 在新轮次单独批准。
- 边界纪律：drivers/、baselines/、binary-manifest.json、监督源码树零改动；不改
  dotfiles；license/1C 不变。
- 后续影响：dsh 的 askpass 附加条件成为 R08 硬性要求；入组试验可能短暂退出图形
  会话，必须有配置原样回退和外部 SSH/TTY 通道。
