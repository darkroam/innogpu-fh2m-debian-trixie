# 030+ 补丁重归纳设计框架（R-F 前置 · 三阶段方案 v24）

> 创建日期：2026-09-05
> 起草：qoder（R-F 起草方）
> 版本：**v24**（codex v10 初审 4 P1 + 1 P2 闭环后；codex v11 初审 2 P1 + 2 P2 闭环后；codex v12 初审 4 P1 + 1 P2 闭环后；codex v13 初审 2 P1 + 2 P2 闭环后；codex v14 初审 2 P1 + 2 P2 闭环后；codex v15 初审 2 P1 + 1 P2 闭环后；codex v16 初审 2 P1 + 1 P2 闭环后；codex v17 初审 2 P1 + 2 P2 闭环后；codex v18 初审 2 P1 + 1 P2 闭环后；codex v19 初审 1 P1 + 1 P2 闭环后；codex v20 初审 3 P1 闭环后；codex v21 初审 1 P1 闭环后；**codex v22 初审 1 P1 闭环后**；codex v23 初审 1 P1 + 1 P2 闭环后；本版：snapshot 备份-回滚协议 + 故障注入测试要求（per codex v10 P1 #1）/ snapshot `--reference-manifest` 统一 CLI + canonicalize/existence check（per codex v10 P1 #2）/ meta.json 路径统一到 `docs/planning/evidence/4.0.2-i3/` + jq 1.7.1 工具链前置（per codex v10 P1 #3）/ 全判定流程引用同一 canonicalize 实现（per codex v10 P1 #4）/ 全文档 v8/v9 当前口径残痕统一为 v10 或 v11，历史变更仅在 §十三 历史对照表中保留（per codex v10 P2 #5）；v12 叠加：备份-回滚-journal 持久化 + 启动恢复机制（per codex v11 P1 #1）/ `--reference-manifest` 实际 CLI 解析（per codex v11 P1 #2）/ v10 当前口径统一为 v11（per codex v11 P2 #3）/ 退出码子命令作用域契约 + journal 损坏退出码 9（per codex v11 P2 #4）；v13 叠加：固定事务目录 `.txn` + 结构化 key=value journal + 恢复校验重绑定（per codex v12 P1 #1）/ 逐字段解析杜绝整行裸比较（per codex v12 P1 #2）/ journal 先于任何旧文件移动、废弃"journal 写失败 → 旧对丢失"场景（per codex v12 P1 #3）/ python3 显式 fsync 文件 + rename + fsync 目录路径、禁止全局 sync（per codex v12 P1 #4）/ 当前状态标签统一 v12（per codex v12 P2 #5）；v14 叠加：OUT_DIR 排他锁 fcntl.flock + 锁获取失败退出码 6（per codex v13 P1 #1）/ staging_dir 路径约束 + journal 字段严格校验（per codex v13 P1 #2）/ 旧对完整性 fail-closed（per codex v13 P2 #3）/ 退出码作用域契约当前标签统一 v14（per codex v13 P2 #4）；v15 叠加：排他锁位置前移到参数解析之后、任何文件系统变更之前（per codex v14 P1 #1）/ journal 计划 + .txn/old.* + OUT_DIR 三者事实一致性 fail-closed + 回滚内容感知去 || true（per codex v14 P1 #2）/ 活动接口 v12/v13 标签统一 v15（per codex v14 P2 #3）/ 三类命名参数统一真实解析 + 未知参数 fail-closed（per codex v14 P2 #4）；v16 叠加：journal 新增 old_tar_sha256 / old_sha_sha256 旧对精确指纹，恢复时严格匹配（未知/损坏/替换内容一律 exit 9，.sha256 精确格式 + hash 字段校验）（per codex v15 P1 #1）/ reconcile 唯一合法入口统一为 `tools/d-stage-audit-gen.py reconcile`，旧 bash 独立脚本降级为历史对照段（per codex v15 P1 #2）/ 活动规范 v8/v10/v12 标签残留统一 v16（per codex v15 P2 #3）；v17 叠加：共享 classify_out_pair() 分类函数（即时回滚与启动恢复共用；sidecar 单行 wc -l==1 + 精确格式 + hash 字段，仅精确 new 可删除）（per codex v16 P1 #1）/ 状态专属不变量（verified/sha_committed 必须完整新对，staged 不得 new，backed_up sha 必须 absent，tarball_committed tar 必须 new；违反 exit 9 不清理证据）（per codex v16 P1 #2）/ 4a 旧对自洽校验（hash 字段 == old_tar_sha256 + sha256sum -c，失败 exit 9）（per codex v16 P2 #3）；**v18 叠加：4a 幂等 no-op 前置（既有对 == staging 新对 → 清理 staging 成功返回，避免旧新指纹相等时恢复必 exit 9）（per codex v17 P1 #1）/ selfcheck_fail 故障注入期望改为 fail-closed（废弃"自动回滚到旧对"预期）（per codex v17 P1 #2）/ 故障注入计数口径修正为 16 场景（per codex v17 P2 #3）/ 活动规范残留 v16/v15 标签统一 v18（per codex v17 P2 #4）；**v19 叠加：fsync 状态依赖数据屏障（staged ← staging 文件+目录 / backed_up ← .txn / 提交与清理 ← OUT_DIR，顺序不可交换，断电一致性）+ 5 个 power-loss 故障注入场景（强制断电，不得用 kill -9 代替）（per codex v18 P1 #1）/ §5.3 bash block 降级为算法伪代码草案 + 唯一实现依据 = 030-d-stage-audit.md §五 v19 Python 契约（per codex v18 P1 #2）/ 幂等 no-op 复用 4a 严格 sidecar 校验 + noop_duplicate_sidecar 故障场景（per codex v18 P2 #3）；**v20 叠加：跨目录 rename 源、目标双目录同步（备份 OUT_DIR→.txn：fsync_dir(.txn) + fsync_dir(OUT_DIR)；回滚 .txn→OUT_DIR：fsync_dir(OUT_DIR) + fsync_dir(.txn)）+ 3 个跨目录 rename 断电窗口故障场景（备份 mv 后 persist 前 / 备份 mv 与双目录 fsync 之间 / 回滚还原 mv 后清理前）+ 故障注入 25 场景 + reconcile 活动规范标签统一 v20（per codex v19 P1 #1 + P2 #2）；**v21 叠加：rolling_back 合法中途态白名单 (new,new)/(absent,new)/(absent,absent)/(old,absent)/(old,old)（统一混合判定仅对非 rolling_back 状态生效）+ 2 个回滚中途窗口故障场景（per codex v20 P1 #1）/ 无 journal 恢复废除"OUT_DIR 自洽即 exit 0"捷径、统一走全新路径由 4a 幂等 no-op 严格收敛 + 1 个 journal 删除后 .txn 残留窗口场景（per codex v20 P1 #2）/ verified 清理顺序 rm old.* → fsync_dir(.txn) → rm journal → rm -rf .txn → fsync_dir(OUT_DIR)（per codex v20 P1 #3）/ 故障注入 28 场景；**v22 叠加：rolling_back 白名单 shell case 修复（未转义 | 是模式分隔符而非状态对字符串，v21 草图的五个白名单状态对实际全部落入拒绝分支 → 改为带引号精确匹配 + Python 契约集合成员判断 rolling_back_mid_state_ok）（per codex v21 P1 #1）；**v23 叠加：回滚还原逐次 mv 屏障（每个跨目录还原 mv 后立即 fsync_dir(OUT_DIR) + fsync_dir(.txn)，防两次还原 mv 之间断电出现旧 tar 双存在/均缺失 exit 9；power_loss_rollback_mid_restore_window 期望改为确定性 (old,absent)）（per codex v22 P1 #1）；**v24 叠加：回滚段逐次 fsync 失败退出码统一 exit 5（两处回滚段 fsync_dir 失败原标 exit 9，与 §五 Python 契约"所有 fsync 失败 exit 5"冲突 → 统一 exit 5，journal 保持 rolling_back 重入收敛）（per codex v23 P1 #1）/ 新增 power_loss_rollback_restore_fsync_window 故障场景（回滚还原 mv 后、任一目录 fsync 未完成断电 → 按被还原文件分列四态矩阵：旧 tar target-only (old,absent)/source-only (absent,absent)；旧 sidecar target-only (old,old)/source-only (old,absent)，一致事实白名单重入收敛；旧文件双存在 / 均缺失 → 三者事实校验 exit 9 保留证据，不得仅凭 OUT 状态对恰为合法中途态而错误接受 sidecar 丢失；29 场景）（per codex v23 P2 #2）**）
> 状态：**设计稿（进行中，未具终审条件）**
> 适用范围：R-F 阶段（fantgpu 5.0.0 基座建立）前置；不直接落代码
> 上下游：上游 = P5 评估（`fantgpu-base-update-evaluation.md` 19 项台账 +
> 59 语义裁决记录 + 435 per-file 证据）；下游 = R-F 第一批 030-NNN 实现
> **三阶段总览**：
> - **阶段一**（Deepin 修改归纳）— D → D_stage，**不依赖 F0**，不生成 030-NNN；
> - **阶段二**（Deepin 基座冻结与 4.0.2-i3 发布）— D_stage → `4.0.2-i3`
>   独立 release commit + annotated tag + 包与源码复现证据；
> - **阶段三**（fantgpu 基座迁移）— 从 `4.0.2-i3` 的中性变更记录出发，逐项与
>   F0 比较、裁决、适配和验证 → O_stage → `5.0.0-i1`。

## 〇、终审前置条件状态（v24 完成时点；分阶段门槛）

> **当前状态：本框架未具终审条件。**
>
> 本框架 v24 已完成 codex v7 返工（8 项 finding 闭环：7 P1 + 1 P2）
> + codex v8 返工（6 P1 + 2 P2 闭环）
> + **codex v9 返工（5 P1 + 3 P2 闭环）**
> + **codex v10 返工（4 P1 + 1 P2 闭环）**
> + **codex v11 返工（2 P1 + 2 P2 闭环）**
> + **codex v12 返工（4 P1 + 1 P2 闭环）**
> + **codex v13 返工（2 P1 + 2 P2 闭环）**
> + **codex v14 返工（2 P1 + 2 P2 闭环）**
> + **codex v15 返工（2 P1 + 1 P2 闭环）**
> + **codex v16 返工（2 P1 + 1 P2 闭环）**
> + **codex v17 返工（2 P1 + 2 P2 闭环）**
> + **codex v18 返工（2 P1 + 1 P2 闭环）**：
>
> - **P1 #1（v19 新口径）**：staged journal 持久化缺少 staging 数据持久化
>   屏障（断电后 state=staged 可能搭配缺失/旧 staging 文件 → 只能 exit 9）
>   → v19 新增 fsync_file / fsync_dir helper + 状态依赖数据屏障（顺序不可
>   交换；staged ← staging 文件+目录 / backed_up ← .txn / 提交与清理 ←
>   OUT_DIR）+ 5 个 power-loss 故障注入场景（强制断电，不得用 kill -9 代替）；
> - **P1 #2（v19 新口径）**：§5.3 bash block 不可直接执行（acquire_snapshot_lock
>   与 persist_journal 定义在调用点之后，直接执行得到 command-not-found）
>   → v19 起降级为算法伪代码草案，唯一实现依据 = 030-d-stage-audit.md
>   §五 v19 Python 契约（helper 签名 + fsync 顺序 + 退出码 + 执行顺序）；
> - **P2 #3（v19 新口径）**：幂等 no-op 绕过 sidecar 严格格式校验（裸
>   sha256sum -c 放行两行重复合法行）→ v19 no-op 复用 4a 严格 sidecar
>   校验结论 + 新增 noop_duplicate_sidecar 故障场景；
>
> + **codex v19 返工（1 P1 + 1 P2 闭环）**：
>
> - **P1 #1（v20 新口径）**：backed_up 屏障遗漏 OUT_DIR 源目录（4b 备份
>   mv 后只 fsync .txn；回滚还原 mv 后只 fsync OUT_DIR）→ 断电后源目录
>   残留目录项 → 旧文件双存在 → 恢复 exit 9，与 backed_up power-loss
>   自动续跑矛盾 → v20 跨目录 rename 源、目标双目录同步（备份：.txn +
>   OUT_DIR；回滚：OUT_DIR + .txn）+ 3 个跨目录 rename 断电窗口故障场景
>   （备份 mv 后 persist 前 / 备份 mv 与双目录 fsync 之间 / 回滚还原 mv
>   后清理前）+ Python 契约屏障顺序同步；
> - **P2 #2（v20 新口径）**：reconcile 活动规范仍保留 v16 当前标签
>   （"reconcile 子命令规范（v16...）"与"活动规范（v16）"）→ v20 统一为
>   当前标签，历史归属保留在括号溯源（per codex v15 P1 #2）；
>
> + **codex v20 返工（3 P1 闭环）**：
>
> - **P1 #1（v21 新口径）**：rolling_back 无法覆盖合法的中途崩溃状态
>   （逐文件删除/还原窗口的 absent/new 等中间事实态会被统一判定拒绝）
>   → v21 统一混合判定仅对非 rolling_back 状态生效 + rolling_back 合法
>   中途态白名单 (new,new)/(absent,new)/(absent,absent)/(old,absent)/
>   (old,old) 显式允许并落入回滚段幂等重入 + 2 个回滚中途窗口故障场景；
> - **P1 #2（v21 新口径）**：无 journal 恢复路径把"OUT_DIR 自洽对"误判
>   为已完成新对 exit 0（无新对指纹证据，旧对完整 + staging 残留会被
>   误判）→ v21 废除该捷径：统一丢弃 .txn 残留后继续全新路径，由 4a
>   幂等 no-op 在严格校验下等价收敛 + power_loss_before_staged_persist
>   期望修订 + 1 个 journal 删除后 .txn 残留窗口场景；
> - **P1 #3（v21 新口径）**：verified 清理缺少 .txn 目录持久化屏障
>   （journal 删除持久但 old.* 删除未持久 → 无 journal 分支 exit 9）
>   → v21 清理顺序改为 rm old.* → fsync_dir(.txn) → rm journal →
>   rm -rf .txn → fsync_dir(OUT_DIR)（4e 与恢复 verified 分支同步）。
>
> + **codex v21 返工（1 P1 闭环）**：
>
> - **P1 #1（v22 新口径）**：rolling_back 白名单的 shell case 表达式实际
>   不匹配任何合法状态（未转义的 | 是模式分隔符而非状态对字符串；
>   `new|new|absent|new|...` 匹配单个 token，五个白名单状态对全部落入
>   拒绝分支 exit 9）→ v22 改为带引号精确匹配
>   `"new|new"|"absent|new"|"absent|absent"|"old|absent"|"old|old"` +
>   audit §五 Python 契约新增可执行等价判定
>   `rolling_back_mid_state_ok`（tuple/set 成员判断，禁止照抄 shell case
>   未转义 | 语法）。
>
> + **codex v22 返工（1 P1 闭环）**：
>
> - **P1 #1（v23 新口径）**：回滚中途还原场景缺少逐次 rename 的持久化
>   屏障（两处回滚段都是两次还原 mv 全部完成后才统一 fsync 源、目标
>   目录 → 第一次 mv 后、第二次 mv 前断电时，(old,absent) 无持久化保证，
>   可能出现旧 tar 双存在或均缺失 → 三者事实校验 exit 9，与
>   power_loss_rollback_mid_restore_window 的无条件自动恢复假设矛盾）
>   → v23 改为**每个**跨目录还原 mv 后立即 fsync_dir(OUT_DIR) +
>   fsync_dir(.txn)（恢复 rolling_back 分支与 4e 即时回滚段同步），
>   power_loss_rollback_mid_restore_window 期望改为确定性 (old,absent)。
>
> + **codex v23 返工（1 P1 + 1 P2 闭环）**：
>
> - **P1 #1**：回滚 fsync 失败退出码契约冲突——两处伪代码将逐次还原后的
>   fsync_dir 失败定义为 exit 9（design:1257/1553），但唯一活动 Python
>   契约规定所有 fsync_file/fsync_dir 失败均为 exit 5（audit:640/668），
>   错误码表 snapshot 5 又只列出 journal/备份 mv 失败（audit:1146）
>   → v24 统一为一个契约：回滚段逐次还原 mv 后的 fsync_dir 失败一律
>   exit 5（与 §五 Python 契约一致，journal 保持 rolling_back 重入收敛），
>   伪代码 / 错误码表（snapshot 5 扩展含回滚还原 fsync 失败）/ 退出码
>   测试说明同步。
> - **P2 #2**：v23 新增的逐次还原屏障缺少屏障未完成窗口测试（现有场景只在
>   第一组双目录 fsync 已完成时断电，只能验证确定性 (old,absent)；备份方向
>   已有 mv 与双目录 fsync 之间断电场景，回滚方向没有）→ v24 新增
>   power_loss_rollback_restore_fsync_window 场景（回滚还原 mv 后、任一
>   目录 fsync 未完成时断电），按**被还原文件**分列四态矩阵：旧 tar →
>   target-only 白名单 (old,absent) / source-only (absent,absent) 且 .txn 有
>   old.*；旧 sidecar（旧 tar 已持久还原）→ target-only 白名单 (old,old) /
>   source-only (old,absent) 且 .txn 有 old.*——一致事实均白名单显式允许
>   重入收敛；旧文件双存在 / 均缺失 → 三者事实校验 exit 9 保留证据
>   （fail-closed，不得仅凭 OUT 状态对恰为合法中途态而错误接受 sidecar
>   丢失）；故障注入 28 → 29 场景，
>   genesis schema 2.3 → 2.4、fault schema 12.0 → 13.0。
>
> - **P1 #1**：D_stage 路径从 `migration/supervised-source-tree/_r16-d-stage/`
>   （绝对保护区）迁出到 `/tmp/r16-d-stage/`（系统 `$TMPDIR`，**不在仓库
>   任何路径下**），阶段二 release commit 通过 tarball 嵌入 D_stage 快照；
> - **P1 #2**：O-2 阶段一输入仅含 D + D_stage 树，**不引用** P5 435/per-file
>   23 BC 矩阵，**不硬编码** F-only 文件名（移至阶段三裁决输入）；
> - **P1 #3**：tree-manifest 命令改用类型分支 + NUL 安全枚举 + 显式
>   `SRC_ROOT` 参数 + 每步 rc + 临时文件 + 原子替换 + 旧产物保护；
> - **P1 #4**：symlink 9 类（**保留 9 类**：跨树 `d2dstage` / `dstage2d`
>   作为与 same-target / target-changed 并列的独立分类；**互斥判定优先级**
>   显式定义）；
> - **P1 #5**：阶段二 release commit 内容清单显式列出 O-2 两个输入
>   manifest + `.sha256` + D_stage 快照 tarball + `.sha256`（9 文件 +
>   2 tarball），`git diff --cached --name-status` 验证机制显式；
> - **P2 #6**：`patches/` 顶层规则由"绝对保护区 + 例外可写"二分法改为
>   **三层规则**（000-029 只读 / 顶层 = 阶段三产物路径 / 4.0.2-i3.*
>   永久禁止）；
> - **P2 #7**：§〇.1 表头与数据行均为 3 列（PASS_DOCS 闭环）；
> - **v8 新增（per codex v7 8 findings）**：
>   - **P1 #1（v8 新口径）**：O-2 F-only 检测**整体删除**（不读 F0 +
>     不持有 F-only 文件清单 → 不可计算）；F-only 检测移到阶段三 O-4 后；
>     §九 退出码 6 删除；§十一 genesis.json `f_only_exclusion_check` 字段删除；
>   - **P1 #2（v8 新口径）**：tree-manifest 命令 SRC_ROOT 通过 `env`
>     显式 export 给 xargs bash -c 子进程（v7 sh -c 子进程不继承调用方
>     shell 变量导致 sha256sum 失败 exit 123）；
>   - **P1 #3（v8 新口径）**：最终 TSV 写入前逐行 awk 校验 filename /
>     symlink_target / file_sha256 字段不含 tab/CR/LF/NUL；readlink 失败
>     立即 exit 5（不再 `|| echo ''` 吞错）；
>   - **P1 #4（v8 新口径）**：manifest schema 检查路径修复 brace
>     expansion（`D{,stage}` = `D` + `stage` ≠ `D_stage`），分别对
>     `D.manifest.tsv` 与 `D_stage.manifest.tsv` 各跑一次 awk NF=4 校验；
>   - **P1 #5（v8 新口径）**：validate-then-replace；删除 v7 旧产物
>     backup（先 backup 后校验路径冲突），全部校验通过后才 mv 到最终路径；
>   - **P1 #6（v8 新口径）**：拆分 8 稳定证据文件（构成 SHA 一致判定
>     基础）+ 1 个运行时 genesis.json（不计 SHA 一致判定，含时间戳 +
>     绝对路径）；
>   - **P1 #7（v8 新口径）**：D_stage 快照 tar.zst 完整可复现规范（固定
>     sort / mtime / owner / group / numeric-owner / no-acls / no-xattrs /
>     no-selinux + zstd 版本/level + tarball ↔ manifest reconcile 命令）；
>   - **P2 #8（v8 新口径）**：阶段二 Git tag 身份（包版本 `4.0.2-i3` vs
>     Git ref `v4.0.2-i3` / `r16-4.0.2-i3` / 其他）须 dsh 在阶段二启动
>     前反查 `git tag -l` 后给唯一名称，详 §五.3 v24（v10 per codex v9 P2 #7 进一步规范：从 `4.0.2-i3.meta.json` `git_tag_ref` 字段读取；v11 per codex v10 P1 #3 进一步规范：meta.json 路径统一到 `docs/planning/evidence/4.0.2-i3/`）。

### 〇.1 分阶段门槛（取代原"任一 O-1..O-4 未完成 → 不具终审条件"）

| 阶段 | 进入本阶段审查的前置条件 | 进入下一阶段的前置条件 |
| --- | --- | --- |
| **阶段一**（D → D_stage） | O-1（19 → Deepin 中性变更记录） + O-2（D → D_stage 完整性审计，含 tree-manifest + symlink 闭合）全闭合 | 阶段二 O-3（运行时 fixture 真机/VM 路径确认） + 双 clean-build 字节一致 + 阶段二验证矩阵全 PASS + 三方一致 + dsh 终审 + 用户阶段二批准 |
| **阶段二**（D_stage → 4.0.2-i3） | 阶段一通过 + 阶段二 O-3（运行时 fixture 真机/VM 路径确认） + 双 clean-build 字节一致 + 阶段二验证矩阵全 PASS + 三方一致 + dsh 终审 + 用户阶段二批准 | 阶段三 O-3（F0 迁移 fixture 真机/VM 路径确认） + O-4（F0 源树就位 + SHA-256 锁定） + 每条 030-NNN 4 项审查 + 阶段三验证矩阵全 PASS + 三方一致 + dsh 终审 + 用户阶段三批准 |
| **阶段三**（4.0.2-i3 + F0 → O_stage → 5.0.0-i1） | 阶段二通过 + 阶段三 O-3（F0 迁移 fixture 真机/VM 路径确认） + O-4（F0 源树就位 + SHA-256 锁定） + 每条 030-NNN 4 项审查 + 阶段三验证矩阵全 PASS + 三方一致 + dsh 终审 + 用户阶段三批准 | 无（阶段三为 R-F 前置终点；后续 R-F 实现由独立 commit / tag 处理） |

**严禁全局断言**：不允许任何"任一 O-1..O-4 未完成 → 不具终审条件"的写法
（v5 §〇 / §八.3 / §十 中已删除）。每个 O-X 必须在哪个阶段才要求，
由上表定义；阶段一**不得**等 O-4 / 阶段三 O-3。

### 〇.2 O-1 / O-2 / O-3 / O-4 各自阶段归属（v7 重新定义 + v8/v9/v10/v11/v12/v13/v14/v15/v16/v17/v18/v19/v20/v21/v22/v23/v24 修订；per codex v6 7 findings + codex v7 P1 #1 F-only 整体后移 + codex v9 P1 #2+#3 目录级 staging + codex v9 P1 #4 `realpath -m` + codex v9 P2 #6 精确版本锁 + codex v9 P2 #7 `$GIT_TAG_REF` 读取 + codex v10 P1 #1+#2+#3+#4 备份-回滚/reference manifest/meta.json 路径/canonicalize 统一 + codex v11 P1 #1+#2 journal + CLI 解析 + codex v11 P2 #3+#4 退出码作用域契约 + codex v12 P1 #1-#4 固定事务目录/结构化 journal/journal 先于移动/fsync 路径 + codex v13 P1 #1+#2 + P2 #3 排他锁/路径约束/旧对完整性 + codex v14 P1 #1+#2 + P2 #3+#4 锁时序/事实一致性/接口标签/CLI 解析 + codex v15 P1 #1+#2 + P2 #3 旧对指纹/reconcile 入口/标签统一 + codex v16 P1 #1+#2 + P2 #3 共享分类/状态不变量/旧对自洽 + codex v17 P1 #1+#2 + P2 #3+#4 幂等 no-op/selfcheck fail-closed/16 场景计数/标签统一 + codex v18 P1 #1+#2 + P2 #3 fsync 屏障/伪代码降级+Python 契约/no-op 严格 sidecar 校验/22 场景 + codex v19 P1 #1 + P2 #2 跨目录 rename 双目录同步/3 断电窗口场景/25 场景/reconcile 标签统一 + codex v20 P1 #1+#2+#3 rolling_back 白名单/无 journal 走全新路径/verified 清理 .txn 屏障/28 场景 + codex v21 P1 #1 白名单可执行等价判定 + codex v22 P1 #1 回滚还原逐次 mv 屏障 + codex v23 P1 #1+P2 #2 回滚段 fsync 退出码统一 exit 5/回滚还原 fsync 窗口场景 29 场景）

| 编号 | 范围 | 阶段归属 | 配套文件 / 输出路径 | 当前状态（v24） |
| --- | --- | --- | --- | --- |
| **O-1** | 19 项台账 → Deepin 中性变更记录（**隔离 F-only**，**不含 F0 决策字段**） | 阶段一 | `docs/planning/030-mapping-table.md`（v12 P5/阶段一/阶段三 三列分离 + F-only excluded-deferred 隔离；v12 保持 v11 政策） | v12 三列分离 + F-only excluded-deferred 隔离（v12 仅配套标签同步 per codex v12 P2 #5；O-1 表本体与 codex v10/v11/v12/v13/v14/v15/v16/v17 全部 finding 无直接关系） |
| **O-2** | D → D_stage 完整性审计（**tree-manifest `SRC_ROOT` 显式 export**，**symlink 9 类闭合 + `realpath -m` canonicalize**，**不读 F0**，**不硬编码 F-only**，**目录级 staging + reconcile-first**，**tar.zst 精确版本锁**，**OUT_DIR 排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal 协议（含旧对精确指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 幂等 no-op（复用严格 sidecar 校验）+ 旧对完整性 fail-closed + fsync 状态依赖数据屏障（断电一致性；跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行等价判定）**，**reconcile 子命令唯一入口**，**`--reference-manifest` CLI 解析**，**退出码子命令作用域契约**，**bash 草案降级伪代码 + Python 实现契约**） | 阶段一 | `docs/planning/030-d-stage-audit.md`（v24 tree-manifest `SRC_ROOT` export + 9 类 symlink 互斥 + `realpath -m` canonicalize + F0 完全隔离 + 9 文件输出 = 8 稳定证据 + 1 运行时 genesis.json + tar.zst 可复现规范 + 目录级 staging + reconcile-first + 排他锁（先于一切变更）+ 固定事务目录 `.txn` + 结构化 journal 协议（含 old_tar_sha256/old_sha_sha256 指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享 classify_out_pair）+ 状态专属不变量 + 旧对自洽校验 + 幂等 no-op（复用严格 sidecar 校验）+ 旧对完整性 fail-closed + fsync 状态依赖数据屏障（跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障）+ rolling_back 合法中途态白名单（rolling_back_mid_state_ok 集合成员判断）+ 回滚还原逐次 mv 屏障 + 无 journal 恢复走全新路径 + reconcile 子命令唯一入口 + `--reference-manifest` CLI 解析（三类命名参数统一解析）+ 退出码作用域契约 + `tools/d-stage-audit-gen.py` 三子命令接口 + §五 Python 实现契约（唯一实现依据）） | v24 已替换 find-exec readlink 命令 + 9 类互斥判定 + `realpath -m` canonicalize 强化 + 6 fixture 覆盖（per codex v9 P1 #4）+ 移除 F-only gate / exit 6 / `f_only_exclusion_check` 字段（F-only 整体移到阶段三 O-4）+ TSV 字段编码 + manifest schema 检查路径 + validate-then-replace + 8/1 文件拆分 + tar.zst 可复现规范 + 目录级 staging + reconcile-first + 三子命令接口 + tar.zst 精确版本锁（per codex v9 P2 #6）+ `$GIT_TAG_REF` 读取（per codex v9 P2 #7）+ 固定事务目录 `.txn` + 结构化 journal + 8 场景故障注入测试要求（per codex v12 P1 #1-#4 journal 重写）+ `--reference-manifest` 实际 CLI 解析（per codex v11 P1 #2）+ 退出码子命令作用域契约 + journal 损坏退出码 9（per codex v11 P2 #4）+ OUT_DIR 排他锁 + 锁获取失败退出码 6（per codex v13 P1 #1）+ staging_dir 路径约束 + journal 字段严格校验（per codex v13 P1 #2）+ 旧对完整性 fail-closed + 11 场景故障注入（per codex v13 P2 #3）+ 排他锁位置前移（per codex v14 P1 #1）+ 三者事实一致性 fail-closed + 12 场景故障注入（per codex v14 P1 #2）+ 旧对精确指纹 + 未知内容 exit 9 + reconcile 子命令唯一入口 + 13 场景故障注入（per codex v15 P1 #1+#2）+ 共享分类函数 + 状态专属不变量 + 旧对自洽校验 + 16 场景故障注入（per codex v16 P1 #1+#2 + P2 #3 + codex v17 P2 #3 计数修正）+ 幂等 no-op + selfcheck_fail fail-closed 期望（per codex v17 P1 #1+#2）+ fsync 状态依赖数据屏障 + bash 草案降级伪代码 + §五 Python 实现契约 + no-op 复用严格 sidecar 校验 + 22 场景故障注入（5 power-loss + 1 no-op 重复行）（per codex v18 P1 #1+#2 + P2 #3）+ 跨目录 rename 源、目标双目录同步 + 3 个跨目录 rename 断电窗口场景 + 25 场景故障注入 + reconcile 活动规范标签统一 v20（per codex v19 P1 #1 + P2 #2）+ rolling_back 合法中途态白名单 + 无 journal 恢复走全新路径 + verified 清理 .txn 屏障 + 3 个新场景 + 28 场景故障注入（per codex v20 P1 #1+#2+#3）+ 白名单可执行等价判定（shell 带引号精确匹配 + Python 集合成员判断）（per codex v21 P1 #1）+ 回滚还原逐次 mv 屏障 + power_loss_rollback_mid_restore_window 期望确定性 (old,absent)（per codex v22 P1 #1）+ 回滚段逐次 fsync 失败 exit 5 统一 + power_loss_rollback_restore_fsync_window 三态契约场景（per codex v23 P1 #1 + P2 #2） |
| **O-3（阶段二子项）** | 阶段二运行时 fixture 真机/VM 路径确认 | 阶段二 | 验证结果存 `docs/planning/evidence/4.0.2-i3/` | 待启动（**不阻塞阶段一**） |
| **O-3（阶段三子项）** | 阶段三 F0 迁移 fixture 真机/VM 路径确认 | 阶段三 | 验证结果存 `docs/planning/evidence/o-stage/` | 待启动（**不阻塞阶段一 / 阶段二**） |
| **O-4** | F0 端（fantgpu 3.3.8.126）源树就位 + SHA-256 锁定 | 阶段三前置（**阶段二完成 + 用户批准后才启动**） | — | 待启动 |

### 〇.3 阶段一统计口径（v6 新增，**不与 P5 19/59/435 混称**）

阶段一与阶段二各自独立的辅助统计量，**禁止用 19 / 59 / 435** 命名：

| 阶段 | 辅助统计量 | 含义 | 来源 |
| --- | --- | --- | --- |
| **阶段一** | **中性记录数** | O-1 产出的 Deepin 中性变更记录条目数（与 P5 59 语义裁决记录**不等价**，多对多映射） | `030-mapping-table.md` 实际闭合行数 |
| **阶段一** | **D→D_stage diff 数** | O-2 `d-stage-audit.tsv` 的 differs 行数 | `docs/planning/evidence/d-stage-audit.tsv` |
| **阶段一** | **D→D_stage symlink 数** | O-2 `d-stage-audit.symlink.tsv` 总条目数 | `docs/planning/evidence/d-stage-audit.symlink.tsv` |
| **阶段二** | **包载荷文件数** | `4.0.2-i3` 包内 payload 的文件计数（与 P5 435 per-file 证据**不等价**） | dpkg-deb -c 输出解析 |
| **阶段二** | **D_stage 包载荷 hash** | `4.0.2-i3` 包载荷的 SHA-256 | sha256sum on 解包 payload |

**严禁表述**：

- "中性记录数 ⊆ 435 per-file 证据" / "中性记录数 = 435 子集" —— 多对多映射，非集合子集；
- "59 个中性记录" —— 59 是 P5 语义裁决记录数，阶段一中性记录数由实际闭合决定；
- "包载荷文件数 = 435" —— 435 是 P5 per-file 证据（432 differs + 3 F-only），
  包载荷是 `4.0.2-i3` 的实际包内容，两者基数与构成均不同。

### 〇.4 F-only 处置隔离（v6 新增；per codex P1 #2 + v8 per codex v7 P1 #1 整体移到阶段三 O-4 + v12 保持 v11 政策）

**F-only 3 文件**（gpu/fant_stackprotector.c、gpu/fant_stackprotector.h、
srvkm/include/common_ri_bridge.h）的处理：

- **不得**进入阶段一 O-1 D_stage 中性记录（不能由 D + 000-029 重放产生）；
- **不得**进入阶段一 O-2 D → D_stage 完整性审计的 differs / identical /
  D-only / D_stage-only 分类（违反 F0 隔离硬约束）；
- **保留为阶段三输入待裁决记录**：写入 O-1 的"阶段三输入占位"段，状态字段
  `excluded-deferred`，不计入阶段一统计量；
- **阶段三启动后**才与 F0 源树比较，按四分类（已覆盖 / 实现更好 /
  实现不同 / 缺失）裁决，最终归入 `absorb` / `adapt` / `rewrite` /
  `base-retain` / `no-030` 之一。

**v8 per codex v7 P1 #1 重大修订**：

- **O-2 阶段一** 不进行 F-only 检测（per codex v7 P1 #1：O-2 不读 F0 +
  不持有 F-only 文件清单 → 检测不可计算）；O-2 §九 退出码 6 已删除；
  O-2 §十一 genesis.json `f_only_exclusion_check` 字段已删除；
- **O-1 阶段一** 仍基于"能否由 D + 000-029 重放产生"判定（不读 F0；
  仅基于本端观察）：F-only 3 文件因为不能由 D + 000-029 重放得到，
  被识别为"excluded-deferred"项；
- **阶段三 O-4 闭合后** 才进行完整 F-only 检测（必须读 F0 才能确认文件
  "仅在 F0 存在"）；
- F-only 文件名清单的**正式提供时机**仍为阶段三裁决阶段（O-4 闭合后），
  由 dsh 提供，**不**在阶段一 O-2 工具生成期间出现。

### 〇.5 D_stage 物化路径 + 保护区边界（v10 per codex v7 P1 #1 + P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 修订）

| 项 | 路径 | 类型 | 说明 |
| --- | --- | --- | --- |
| **D 源树根** | `third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2`（仓库内只读快照） | 阶段一输入 | 只读输入；脚本只读，不得修改 |
| **D_stage 物化路径** | `/tmp/r16-d-stage/`（**绝对路径**，**不在仓库任何路径下**） | 阶段一输出 | 系统临时目录（`$TMPDIR`）；非仓库路径，**不与** drivers/ / baselines/ / patches/ / `migration/supervised-source-tree/` 任何子路径冲突；阶段一脚本**全权读写** |
| **阶段二 release commit 内容** | 阶段一产出物（`docs/planning/evidence/d-stage-audit.tsv*`、`030-mapping-table.md` 闭合、`4.0.2-i3.meta.json` + 双构建证据）+ **`/tmp/r16-d-stage/` 的 D_stage 完整快照以 tarball 形式嵌入 `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst`（独立路径，不在保护区清单内）** | 阶段二输出 | commit 仅含：方案文档 + 证据 + 元数据 + D_stage 快照；**不含**任何 030-NNN / F0 内容 |
| **阶段二 release commit 形成规则** | 1. 阶段一产物全部就位 + 双 clean-build 复现通过；2. 三方一致 + dsh 终审 + 用户批准；3. **单次独立 commit**（不与阶段一文档 commit / O-2 工具 commit 混合）；4. commit message 必须包含"阶段二 release commit for 4.0.2-i3" 关键字 + 关联 evidence 路径 + SHA-256 | 阶段二流程 | 严禁 amend / 修改既有 commit |

**v10 D_stage 物化路径硬约束（per codex v6 P1 #1 + P1 #5 + codex v7 P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6）**：

- **绝对不写入** `migration/supervised-source-tree/` 任何子路径（per
  `docs/project/multiagent-collab.md:40-41` 监督分支绝对保护区，**任何
  dsh 批准都不能把其子路径改称非保护区**；v6 此写法已删除）；
- D_stage 物化根 = **`/tmp/r16-d-stage/`**（系统 `$TMPDIR`，不与仓库
  任何路径冲突；阶段一脚本可全权读写，阶段一完成后须将 D_stage 完整
  快照打包为 `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst`
  嵌入阶段二 release commit，不再依赖 `/tmp` 持久性）；
- 阶段二 release commit 内容**仅包含**：方案文档 + 阶段一证据（不含
  D_stage 快照 tarball 因为它属于阶段二产物）+ 阶段二产物（**含**
  D_stage 快照 tarball + O-2 两个输入 manifest + 各自 `.sha256`，per
  §五.2 v10 修订）+ 元数据 + 双构建证据；
- D_stage 物化根的任何修改必须由 O-1 / O-2 工具生成，**禁止**手填；
- 阶段一完成后，`/tmp/r16-d-stage/` 在 O-2 工具生成 release commit
  tarball 后**可清理**（已嵌入 commit，不再需要本机临时目录）；
- 阶段三**仅从 `4.0.2-i3` tag 出发**，从阶段二 release commit 中解压
  `d-stage-snapshot.tar.zst` 重建 D_stage（如需），**不依赖**
  `/tmp/r16-d-stage/` 也不再写入 `/tmp/r16-d-stage/`。

### 〇.6 patches/ 三层规则（v8 per codex v7 P2 #6 reconcile + v8 闭合复核）

| 路径层 | 类型 | 写入规则 |
| --- | --- | --- |
| **`patches/000-029/`**（Deepin 既有 000-029 patch 系列子目录） | **只读** | 阶段一 / 阶段二 / 阶段三**全部禁止写入**；阶段一脚本只读取 patch 内容作为输入 |
| **`patches/` 顶层（不含 `patches/000-029/`）** | **阶段三产物路径** | 阶段一 / 阶段二**禁止写入**；阶段三允许写入 `patches/030-NNN.{patch,meta.json}`（O-4 闭合后 + 用户阶段三批准）；**不允许**写入 `patches/4.0.2-i3.*` 或 `patches/<其他>` |
| **`patches/4.0.2-i3.*`** | **永久禁止** | v5 §五.4 / §十一 中"如仓库约定允许写入 patches/4.0.2-i3.*"的模糊表述已删除；阶段二 release 元数据走 `4.0.2-i3.meta.json` + 双构建证据走 `docs/planning/evidence/4.0.2-i3/` |

**v8 reconcile 关键变化（v7 三层规则延续，v8 闭合复核无新增修改）**：

- **删除** v6 "顶层 = 绝对保护区 + `patches/030-NNN.*` 阶段三允许 +
  `patches/4.0.2-i3.*` 永久禁止"的**二分法逻辑矛盾**（既绝对保护又可例外）；
- **改为** "三层规则"：每层路径政策**预先显式定义**，**不再依赖** dsh
  后续例外审批；
- **`patches/` 顶层（不含 000-029）= 阶段三产物路径**，**不是**绝对
  保护区，因为阶段三明确允许写入 `patches/030-NNN.{patch,meta.json}`；
- **`patches/4.0.2-i3.*` 永久禁止**：路径在 `patches/` 下但不在允许清单
  `patches/030-NNN.*` 中，所以**任何阶段都禁止**；
- **绝对保护区**清单见 §十一，不含 `patches/` 顶层本身（仅含
  `patches/000-029/` 只读子目录）。

## 一、流程图（三阶段硬约束）

```
                              P5 评估产出（不可变）
                              ─────────────────
                              19 项台账 + 59 语义裁决记录 + 435 per-file 证据
                                        │
                ┌───────────────────────┴───────────────────────┐
                │                                               │
        【阶段一】 Deepin 修改归纳                              │
        D → D_stage（D + 有效 Deepin 000-029 patch）             │
        产出：Deepin 中性变更记录（每条对应 1 个实际生效修改）   │
        门禁：O-1（19 → 中性记录） + O-2（D→D_stage 完整性）     │
        禁止：不得读 F0 / 不得做 D→F0 裁决 / 不得生成 030-NNN   │
                │                                               │
                ▼                                               │
        【阶段二】 Deepin 基座冻结与 4.0.2-i3 发布              │
        D_stage → 4.0.2-i3                                      │
        产出：源码 commit SHA + 有效 patch 清单 + 包 SHA-256     │
              + 双构建复现 + 测试结果 + 安装回退说明            │
        + 独立 release commit + 不可移动的 annotated tag         │
                │                                               │
                ├───────────────┬───────────────────────────────┤
                ▼               ▼                               ▼
        Deepin continuation  【阶段三】 fantgpu 迁移            （未来其它路线）
        （从 4.0.2-i3 另开    从 4.0.2-i3 中性记录出发          （待用户决策）
         分支继续维护）        + F0（fantgpu 3.3.8.126）
                              逐项裁决（covered / better /
                                different / missing）
                              + 许可证审查 + 依赖审查 + O_stage 适配
                              产出：030-NNN × N + O_stage + 5.0.0-i1
                              门禁：O-3（阶段三 F0 迁移测试 fixture）
                                   + O-4（F0 源树锁定）
```

**关键不变量**：

- 阶段一**不读 F0**（文件内容 / SHA / 路径等任何引用均禁止）；
- 阶段二**不引入 F0 内容**，**不生成 030-NNN**；tag 名称按仓库现有命名
  确认（不得凭空声明不存在的 tag）；
- 阶段三**仅**从 `4.0.2-i3` 中性变更记录出发，F0 源树锁定后才开始；
- 三阶段各自独立 release commit / tag / 测试证据，**不互相替代**；
- 阶段二完成后，从 `4.0.2-i3` 分出 Deepin continuation 与 F0/R-F 迁移
  两条**独立血缘**（互不混入）。

## 二、命名与边界（四术语 + 两 tag，强约束）

| 术语 | 含义 | 阶段归属 | 来源 / 角色 |
| --- | --- | --- | --- |
| **D** | Deepin 原始 | 阶段一输入 | Deepin 4.0.x 源树基线；P5 评估的对照起点；整体归档（保留索引，不删除不重写） |
| **D_stage** | Deepin-derived staging | 阶段一输出 | 阶段一当前 Deepin-derived staging 树；`D_stage = D + 已应用的 Deepin 000-029 有效 patch`；最终演化为 `4.0.2-i3` |
| **F0** | fantgpu 原始 | 阶段三输入 | fantgpu 3.3.8.126 源树快照；5.0.0-i1 的基座；030+ 补丁全部相对 F0 应用 |
| **O_stage** | R-F staging | 阶段三输出 | 阶段三当前 staging 树；`O_stage = F0 + 已批准的 030-NNN 补丁`；最终演化为 5.0.0-i1 |
| **`4.0.2-i3`** | Deepin 血缘终止/冻结 tag | 阶段二输出 | 阶段二 release commit + annotated tag；Deepin 血缘终止点；后续两条路线（Deepin continuation + F0/R-F 迁移）的共同起点 |
| **`5.0.0-i1`** | R-F 释放 tag | 阶段三输出 | 阶段三最终 release；`O_stage 最终态 = 5.0.0-i1` |

**全文与文档严格使用 D / D_stage / F0 / O_stage + 4.0.2-i3 / 5.0.0-i1
六术语**，不得再用：

- "O" 表示 Original fantgpu（已废止，必须改 F0）或 Deepin-derived
  staging（已废止，必须改 D_stage）；
- "staging" 单独使用（必须明确为 D_stage 或 O_stage）；
- "Deepin" / "fantgpu" 等英文原名混用时需附（= D） / （= F0）锚定；
- 不存在的 tag / commit（必须从仓库既有命名 + git 验证反查）。

## 三、统计口径（三个数字，绝不混称；分阶段语义）

### 3.1 三个数字的定义（不变，仍为 P5 事实）

| 数字 | 名称 | 含义 | 单位性质 | 来源 |
| --- | --- | --- | --- | --- |
| **19** | **台账项** | P5 评估产出的台账条目 | **台账条目数** | `fantgpu-base-update-evaluation.md` line 349-371 |
| **59** | **语义裁决记录** | 19 项台账展开后的语义裁决条目（44 adapted-port + 7 已覆盖 + 6 retain + 2 runtime-verify） | **裁决条目数** | 台账项 → 裁决级展开（O-1 产物） |
| **435** | **per-file 证据** | P2 manifest 提供的 per-file 分类证据（432 differs + 3 F-only） | **文件数** | `tools/r16-classify.py` 输出 |

### 3.2 三个数字的阶段无关性（v6 固定 P5 口径）

**严禁**：19 / 59 / 435 在阶段一 / 阶段二 / 阶段三的语境中**重新赋予含义**。

- **19** 仅指 P5 台账项数；阶段一不预生成 19 条中性变更记录（多对多映射）；
  阶段二 patch 清单条目数 = 实际 `applied` 处置条目数（**不预设**等于 19）；
  阶段三 = P5 19 项台账逐项比较的源项。
- **59** 仅指 P5 语义裁决记录数（来自 P5 line 78 校验 7+6+2+44=59）；阶段一
  中性记录数由 O-1 实际闭合决定（**不等于** 59）；阶段二 `4.0.2-i3` 元数据
  中记录的中性裁决条目数同样**不预设**等于 59。
- **435** 仅指 P5 per-file 证据（432 differs + 3 F-only，`tools/r16-classify.py`
  输出）；阶段一 D→D_stage diff 数 = O-2 实际生成的 differs 行数
  （**不等于** 435，**不预设**等于 432）；阶段二包载荷文件数 = `4.0.2-i3`
  包内容（**不等于** 435，**不含** 3 F-only）；阶段三追溯到 D 端的 per-file
  证据子集（**不等于** 435）。

阶段一 / 阶段二 / 阶段三的辅助统计量定义见 §〇.3；任何试图用 19 / 59 / 435
重新指代阶段产物的写法**禁止**。

**禁止表述**：

- "59 ⊆ 435" / "59 是 435 的子集" —— 集合论表述错误；
- "59 个文件" / "59 文件级语义判定项" —— 59 不是文件数；
- "59 裁决记录 ⊆ 435 per-file 证据"（多对多映射，非集合子集）；
- 任何把 59 等同于 435 子集的简化混称。

## 四、阶段一：Deepin 修改归纳（D → D_stage）

### 4.1 阶段一输入

| 输入 | 说明 | 来源 / 物化路径 |
| --- | --- | --- |
| **D 源树** | Deepin 4.0.x 原始源树（SHA-256 锁定，**只读**） | `third_party/innogpu-fh2m-deepin-202504/root/usr/src/innogpu-kernel-2.2`（仓库内只读快照；脚本只读，不得修改） |
| **当前 Deepin-derived staging** | 仓库现有的 Deepin 修改 staging 树 | 仓库当前 working tree / branch |
| **D_stage 物化根**（阶段一输出） | **非保护区**临时输出根（**v8 per codex v6 P1 #1 + codex v7 P1 #7**：已从 `migration/supervised-source-tree/_r16-d-stage/` 迁出；tar.zst 可复现规范详 §五.3） | **`/tmp/r16-d-stage/`**（系统 `$TMPDIR`，不与仓库任何路径冲突）；阶段一完成后须将 D_stage 完整快照打包为 `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst` 嵌入阶段二 release commit（详 §〇.5 + §五.2 + §五.3） |
| **现有 000-029 patch** | Deepin 血统下既有 patch 系列（000-029） | `patches/000-029`（只读） |
| **P5 台账** | 19 项台账 + 59 语义裁决记录 + 435 per-file 证据 | `fantgpu-base-update-evaluation.md` |
| **实际有效 patch 集合** | 当前 Deepin-derived staging 中实际生效的 patch 清单 | 由 O-1 反查得到 |

**D_stage 物化路径硬约束（v8 per codex v6 P1 #1 + codex v7 P1 #7 修订）**：

- **不得**写入 `drivers/` / `baselines/` / `binary-manifest.json` / `patches/`（顶层）/
  `debs/` / `vendor/` / `build/` / `third_party/` / `migration/supervised-source-tree/`
  （**顶层为绝对保护区**，**任何子路径均不得写入**，**`migration/supervised-source-tree/_r16-d-stage/` 在 v6 中已取消**，v6 §〇.5 / §四.1 / §十一中把该子路径改称"非保护区"违反 `docs/project/multiagent-collab.md:40-41` 监督分支绝对保护区规则）；
- D_stage 物化根 = **`/tmp/r16-d-stage/`**（系统 `$TMPDIR`，**v24 唯一合法
  路径**，不与任何仓库保护区路径冲突；阶段一脚本可全权读写）；
- D_stage 物化根的任何修改必须由 O-1 / O-2 工具生成，**禁止**手填；
- 阶段一完成后，D_stage 完整快照通过 O-2 工具打包为
  `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst` 嵌入阶段二
  release commit（**v10 per codex v7 P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6**：tar.zst 可复现规范详 §五.3）；
- 阶段三**仅从 `4.0.2-i3` tag 出发**，如需重建 D_stage，从阶段二 release
  commit 的 tarball 中解压；**不再写入 `/tmp/r16-d-stage/`**。

### 4.2 阶段一输出

- **D_stage 源树**：D + 实际生效 Deepin patch 应用后的当前 staging；
- **Deepin 中性变更记录**（每条对应 1 个实际生效修改）：
  - 来源 patch（000-029 中的具体 patch 编号 + SHA-256）；
  - 目标路径（D_stage 源树中的具体路径）；
  - 语义（修改做什么、为什么、改动范围）；
  - 依赖（前序 patch / 反向 patch / 共享结构 / 共享回调）；
  - 顺序（应用顺序约束：必须在哪些 patch 之前 / 之后）；
  - 许可证证据（来源 SPDX + 上游 notice）；
  - 验证入口（静态 + 运行时判据）；
  - **处置**：`applied` / `duplicate`（与其他 patch 重复）/ `no-op`（实际无效果）/
    `pure-rename`（仅路径重命名）/ `invalid`（P5 判定为无效）；
  - **不包含**任何 F0 比较 / 裁决 / 030-NNN 生成字段。

### 4.3 阶段一判定准则

> 每个**实际生效**的修改必须**恰好归属一个** Deepin 中性变更记录；**否则**
> 显式登记为 `duplicate` / `no-op` / `pure-rename` / `invalid`。
>
> 中性变更记录必须**能够重放得到当前 Deepin-derived staging**（即：从 D +
> 记录中声明的有效 patch 清单 + 应用顺序 + 验证 → 可重现当前 D_stage）。

### 4.4 阶段一允许 / 禁止

| 维度 | 允许 | 禁止 |
| --- | --- | --- |
| **输入** | D 源树、当前 Deepin-derived staging、000-029 patch、P5 台账 | F0 任何引用（路径 / SHA / 内容 / 命名空间） |
| **决策** | Deepin patch 有效性 / 重复 / 重命名 / 无效 | absorb / adapt / rewrite / base-retain / no-030（任何 F0 决策） |
| **产物** | D_stage 源树 + Deepin 中性变更记录 | 030-NNN / `patches/030-NNN.patch` / `patches/030-NNN.meta.json` |
| **写入路径** | 阶段一文档层（`docs/planning/`）+ D_stage 源树 | 任何保护区路径（详 §十一） |

### 4.5 阶段一门禁

- **O-1**：19 项台账 → Deepin 中性变更记录
  - 见 `docs/planning/030-mapping-table.md`（v5 重定义为 Deepin 中性变更记录
    映射表，每行 1 个实际生效修改 + 处置字段）；
- **O-2**：D → D_stage 完整性审计
  - 见 `docs/planning/030-d-stage-audit.md`（v5 新增，验证阶段一确实收齐了
    当前 Deepin 修改；与原 D→F0 宽 diff **无关**）；
- **门禁通过判定**（任一未闭合即不通过）：
  - 19 个台账条目全部展开为 Deepin 中性变更记录；
  - 中性变更记录可重放得到当前 D_stage 源树（逐字段回放测试通过）；
  - O-2 完整性审计通过（无 hidden change / 无 missing patch）。

## 五、阶段二：Deepin 基座冻结与 `4.0.2-i3` 发布（D_stage → 4.0.2-i3）

### 5.1 阶段二输入

| 输入 | 说明 | 来源 |
| --- | --- | --- |
| **D_stage 源树** | 阶段一输出（SHA-256 锁定） | 阶段一产物 |
| **Deepin 中性变更记录** | 阶段一输出（O-1 闭合） | `030-mapping-table.md` |
| **D → D_stage 完整性审计** | 阶段一输出（O-2 闭合） | `030-d-stage-audit.md` |

### 5.2 阶段二输出（必填字段）

| 输出 | 说明 | 存储位置 |
| --- | --- | --- |
| **源码 commit SHA** | `4.0.2-i3` release commit 的 git SHA-256 | release commit 元数据 |
| **完整有效 patch 清单** | 阶段一中性变更记录的全部 `applied` 处置条目（按顺序） | release commit 元数据 + `4.0.2-i3.meta.json` |
| **包名** | 包名（按仓库既有命名约定） | `4.0.2-i3.meta.json` |
| **包 SHA-256** | 完整包的 SHA-256 哈希 | `4.0.2-i3.meta.json` |
| **epoch / mtime 规则** | dpkg 确定性构建所需 | `4.0.2-i3.meta.json` |
| **构建命令** | 完整可重放的构建命令序列 | `4.0.2-i3.meta.json` |
| **双构建结果** | 同输入同 epoch 两个全新构建根逐字节一致（**双 clean-build 复现**） | `docs/planning/evidence/4.0.2-i3/`（非保护区，详 §七.1） |
| **安装 / 回退说明** | dpkg 安装命令 + 回退到 D 的命令 | `4.0.2-i3.meta.json` |
| **完整测试结果** | 阶段二验证矩阵全 PASS（详 §七.1） | `docs/planning/evidence/4.0.2-i3/` |

**`4.0.2-i3` release commit 内容（v8 per codex v6 P1 #5 + codex v7 P1 #1 + P1 #7 修订；不含 `patches/` / 不含 `migration/supervised-source-tree/`）**：

```
release commit 必须包含且仅包含以下 4 类内容（**禁止** 修改 / 提交 drivers/ /
baselines/ / binary-manifest.json / patches/ 任何内容 / **禁止** 提交
migration/supervised-source-tree/ 任何子路径 per
docs/project/multiagent-collab.md:40-41 绝对保护区）：

1. 方案文档：
   - docs/planning/030-patch-rederivation-design.md（v24 闭合版）
   - docs/planning/030-mapping-table.md（O-1 v12 闭合版）
   - docs/planning/030-d-stage-audit.md（O-2 v24 闭合版）

2. 阶段一证据（docs/planning/evidence/d-stage-audit.* 八件套 + O-2 输入
   lock；**v10 per codex v6 P1 #5 + codex v7 P1 #6 + codex v9 P1 #2 + P1 #3 修订**：8 稳定证据
   + 1 运行时 genesis.json = 共 9 文件；目录级 staging + reconcile-first 提交；详 §五.3 v24）：
   - docs/planning/evidence/d-stage-audit.tsv
   - docs/planning/evidence/d-stage-audit.tsv.sha256
   - docs/planning/evidence/d-stage-audit.genesis.json
   - docs/planning/evidence/d-stage-audit.symlink.tsv
   - docs/planning/evidence/d-stage-audit.symlink.tsv.sha256
   - docs/planning/evidence/d-stage-audit.D.manifest.tsv        （O-2 D 输入 lock）
   - docs/planning/evidence/d-stage-audit.D.manifest.tsv.sha256 （O-2 D 输入 lock hash）
   - docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv         （O-2 D_stage 输入 lock）
   - docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv.sha256  （O-2 D_stage 输入 lock hash）

3. 阶段二产物：
   - docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json
     （release 元数据：包名/SHA-256/epoch/构建命令/patch 清单/
     安装回退说明/dsh 确认的 git_tag_ref，全文 JSON；
     **v11 per codex v10 P1 #3**：路径**统一**在 `docs/planning/evidence/4.0.2-i3/`
     目录下，与 §9.2 F0 迁移起点的 read 路径一致；
     必须用 jq 1.7.1+ 解析 — v11 把 jq 纳入工具链前置条件
     写入 genesis.json `tool_versions.jq`）
   - docs/planning/evidence/4.0.2-i3/ 目录下：
     - build-A.sha256 / build-B.sha256  （双 clean-build 字节一致证据）
     - run-test-results.txt  （阶段二验证矩阵全 PASS 记录）
     - install-rollback.txt  （dpkg 安装/回退验证证据）
     - d-stage-snapshot.tar.zst  （v11：D_stage 完整快照；zstd 压缩；
        由 O-2 `snapshot` 子命令从 /tmp/r16-d-stage/ 打包生成；**v11 per codex v7 P1 #7**
        （v8/v9）+ **v11 per codex v8 P1 #1**（zstd 版本解析 / `--no-seclinux` 删除）+ **v11 per codex v8 P1 #4**（snapshot hash 固定最终 basename）+ **v11 per codex v8 P2 #6**（精确版本锁定 tar 1.35 + zstd 1.5.7）+ **v11 per codex v9 P1 #2 + P1 #3**（目录级 staging + reconcile-first）+ **v11 per codex v10 P1 #1**（备份-回滚协议 + 故障注入测试）+ **v11 per codex v10 P1 #2**（snapshot 显式 --reference-manifest + canonicalize/existence check）+ **v11 per codex v10 P1 #3**（jq 1.7.1 工具链前置 + meta.json 路径统一到 evidence dir）
        d-stage-snapshot.tar.zst.sha256）
     - d-stage-snapshot.tar.zst.sha256  （D_stage 快照 hash）

4. D_stage 包源快照（**v10 per codex v7 P1 #1 + P1 #5 + P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 修订**）：
   - 路径 = `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst`
     （**唯一合法嵌入路径**；不再写入 migration/supervised-source-tree/
     任何子路径，违反监督分支绝对保护区）
   - 阶段一完成后由 O-2 工具从 /tmp/r16-d-stage/ 打包（zstd 压缩）
   - 任何写入都必须在 O-1 / O-2 工具生成范围内，禁止手填
   - O-2 工具必须：1) 对 tarball 计算 SHA-256；2) 写入 .sha256 文件；
     3) 验证 tarball 字节流与 d-stage-audit.D_stage.manifest.tsv 列出的
     文件一致（**v10 per codex v7 P1 #7 + codex v8 P1 #2 + P1 #3 + codex v9 P1 #1 + P1 #2 + P1 #3**：tarball ↔ manifest reconcile 命令
     显式定义，详 §五.3 v24；4 字段完整 + 全局排序 + reconcile-first 提交）；4) 写入完成后 /tmp/r16-d-stage/ 可清理（已嵌入 commit）

### 5.3 v24 D_stage 快照 tar.zst 可复现规范（per codex v7 P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + P2 #7 + codex v10 P1 #1+#2 + codex v11 P1 #1+#2 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2）

**目的**：保证不同机器 / 不同时间两次生成的 `d-stage-snapshot.tar.zst`
字节一致（同一输入），从而 `d-stage-snapshot.tar.zst.sha256` 可作为
release commit 的可验证锁。

**tar.zst 算法参考（v19 起降级为非执行伪代码，per codex v18 P1 #2）**：

> v18 曾把本 block 标为"显式命令（必填、必须严格遵守）"，但 block 内
> `acquire_snapshot_lock`（:500 调用）与 `persist_journal`（:1182 首次调用）
> 均以 python3 语义定义于 block 后段注释，按 bash 直接执行会得到
> command-not-found。**v19 起本 block 定位 = 算法伪代码草案**（bash 语法
> 仅作算法示意，不作为可直接执行脚本）；唯一可执行入口 =
> `tools/d-stage-audit-gen.py snapshot`（python3），**实现唯一依据 =
> `030-d-stage-audit.md` §五 v19 Python 契约**（helper 签名 + fsync 顺序 +
> 退出码 + 恢复检查先行于 staging 生成的执行顺序）。

```bash
#!/usr/bin/env bash
# v19 d-stage-snapshot.tar.zst 算法伪代码草案（per codex v7 P1 #7 + codex v8 P1 #1 + P1 #4 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v14 P1 #1 + P2 #4 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3）
# 注意（v19 per codex v18 P1 #2）：本 block 非可直接执行脚本；helper
# （acquire_snapshot_lock / fsync_file / fsync_dir / persist_journal /
# classify_out_pair）语义以 030-d-stage-audit.md §五 Python 契约为准，
# 实际执行顺序 = Step 0 锁 → 恢复检查 → 全新路径 staging 生成（本 block
# 线性排版仅为算法示意，不代表真实执行顺序）。
# 输入（命名参数）：--d-stage-root <path>（必填）+ --out-dir <path>（必填）
#   + [--reference-manifest <path>]（可选，有默认值）
# 输出：${OUT_DIR}/d-stage-snapshot.tar.zst (+ .sha256)
# 硬约束：LC_ALL=C + GNU tar 1.35 + zstd 1.5.7 + 固定 sort/mtime/owner/group

set -euo pipefail
LC_ALL=C
export LC_ALL
export TZ=UTC

# === 完整命名参数解析（v15 per codex v14 P2 #4：此前草图从 $1/$2 读位置
# 参数，与 --reference-manifest 解析不一致且 --d-stage-root/--out-dir 值
# 被当未知参数忽略）。三类路径参数统一真实解析；未知参数 / 缺值 / 缺必填
# 参数一律 fail-closed exit 1（不再静默忽略）。 ===
D_STAGE_SRC_ROOT=""
OUT_DIR=""
REFERENCE_MANIFEST_RAW=""
while [ $# -gt 0 ]; do
  case "$1" in
    --d-stage-root=*) D_STAGE_SRC_ROOT="${1#--d-stage-root=}" ;;
    --d-stage-root)
      [ $# -ge 2 ] || { echo "ERROR: --d-stage-root 缺少参数值" >&2; exit 1; }
      D_STAGE_SRC_ROOT="$2"; shift ;;
    --out-dir=*) OUT_DIR="${1#--out-dir=}" ;;
    --out-dir)
      [ $# -ge 2 ] || { echo "ERROR: --out-dir 缺少参数值" >&2; exit 1; }
      OUT_DIR="$2"; shift ;;
    --reference-manifest=*) REFERENCE_MANIFEST_RAW="${1#--reference-manifest=}" ;;
    --reference-manifest)
      [ $# -ge 2 ] || { echo "ERROR: --reference-manifest 缺少参数值" >&2; exit 1; }
      REFERENCE_MANIFEST_RAW="$2"; shift ;;
    *)
      echo "ERROR: snapshot 未知参数：$1 → exit 1（fail-closed，不静默忽略）" >&2
      exit 1 ;;
  esac
  shift
done
[ -n "$D_STAGE_SRC_ROOT" ] || { echo "ERROR: 缺少必填参数 --d-stage-root" >&2; exit 1; }
[ -n "$OUT_DIR" ] || { echo "ERROR: 缺少必填参数 --out-dir" >&2; exit 1; }

# === Step 0：OUT_DIR 排他锁（v15 per codex v14 P1 #1：锁必须在任何文件
# 系统变更之前获取——v14 草图在 reconcile 之后才加锁，两个进程仍可能在
# 加锁前共享/覆盖 .txn/staging。此处锁先于临时文件、.txn、staging 生成
# 与 reconcile 的一切动作）。 ===
# v19 per codex v18 P1 #2：acquire_snapshot_lock 为 python3 helper（签名
# 与语义见 030-d-stage-audit.md §五 Python 契约 + 本 block 后段注释），
# 伪代码不内联 bash 定义；真实实现不得以未定义命令直接执行。
acquire_snapshot_lock "${OUT_DIR}" || {
  echo "ERROR: OUT_DIR 排他锁获取失败（另一 snapshot 进程持有 ${OUT_DIR}/.snapshot.lock）→ exit 6" >&2
  exit 6
}

# 工具版本校验（v9 per codex v8 P1 #1 修订：用 regex 提取版本号，不再
# 用 awk $2；本机 `zstd --version | head -1` 输出 `Zstandard v1.5.x`，
# $2 取到字面量 `Zstandard` 导致版本校验错误退出）
TAR_VER=$(tar --version | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
ZSTD_VER=$(zstd --version 2>&1 | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
if [ -z "$TAR_VER" ] || [ -z "$ZSTD_VER" ]; then
  echo "ERROR: failed to parse tar/zstd version (tar='$TAR_VER' zstd='$ZSTD_VER')" >&2
  exit 78
fi
# v10 per codex v9 P2 #6：精确版本拒绝（本机实测 tar 1.35 + zstd 1.5.7）。
# 任何 tar != 1.35 或 zstd != 1.5.7 一律 exit 78（不再用最低版本约束）；
# 这是为跨机器 SHA 一致：不同 GNU tar / zstd 版本可能产生不同 header /
# 压缩字节流，仅精确锁定才能保证 reference SHA 可重放。
REQUIRED_TAR_VER="1.35"
REQUIRED_ZSTD_VER="1.5.7"
if [ "$TAR_VER" != "$REQUIRED_TAR_VER" ]; then
  echo "ERROR: tar version must be exactly $REQUIRED_TAR_VER (got $TAR_VER)" >&2
  echo "  v10 per codex v9 P2 #6: cross-machine SHA reproducibility requires exact tool version" >&2
  echo "  替换为同版本 tar 或重新生成 reference snapshot（详 genesis.json snapshot_boundary）" >&2
  exit 78
fi
if [ "$ZSTD_VER" != "$REQUIRED_ZSTD_VER" ]; then
  echo "ERROR: zstd version must be exactly $REQUIRED_ZSTD_VER (got $ZSTD_VER)" >&2
  echo "  v10 per codex v9 P2 #6: cross-machine SHA reproducibility requires exact tool version" >&2
  echo "  替换为同版本 zstd 或重新生成 reference snapshot（详 genesis.json snapshot_boundary）" >&2
  exit 78
fi

EPOCH=@1640995200  # 2022-01-01 00:00:00 UTC 固定时间戳

TMP_TAR="${OUT_DIR}/.d-stage-snapshot.tar.zst.tmp.$$"
TMP_SHA="${OUT_DIR}/.d-stage-snapshot.tar.zst.sha256.tmp.$$"
trap 'rm -f -- "$TMP_TAR" "$TMP_SHA"' EXIT

# v9 P1 #1 关键选项：
#   --sort=name              文件名排序（确定性）
#   --mtime=$EPOCH           固定 mtime（避免时间戳漂移）
#   --owner=0 --group=0      固定 owner/group
#   --numeric-owner          不解析用户名（避免主机差异）
#   --no-acls --no-xattrs --no-selinux
#                            不保留任何扩展属性（v9 per codex v8 P1 #1：
#                            `--no-seclinux` 不存在于 GNU tar，已删除；
#                            三选项已覆盖 POSIX ACL + xattr + SELinux context）
#   --transform='s,^\.,d-stage,'
#                            tar 内路径前缀改为 d-stage（避免绝对路径污染）
#   -cf -                    输出到 stdout（管道给 zstd）
#   | zstd -q -19            zstd level 19（可复现 + 高压缩）
tar --sort=name \
    --mtime="$EPOCH" \
    --owner=0 --group=0 \
    --numeric-owner \
    --no-acls --no-xattrs --no-selinux \
    --transform='s,^\.,d-stage,' \
    -C "$D_STAGE_SRC_ROOT" \
    -cf - . \
  | zstd -q -19 \
  > "$TMP_TAR"

# SHA-256 计算 + 写入 .sha256（v9 per codex v8 P1 #4 修订：
# 对 TMP_TAR 计算 hash（保证 hash 与文件内容匹配）但 .sha256 文件
# 中固定写入最终 basename `d-stage-snapshot.tar.zst`，rename 后
# `sha256sum -c` 才能通过）
LC_ALL=C sha256sum "$TMP_TAR" \
  | LC_ALL=C awk -v f="d-stage-snapshot.tar.zst" '{print $1"  "f}' \
  > "$TMP_SHA"

# === v13 目录级 staging + reconcile-first（v10 per codex v9 P1 #2 + P1 #3 引入，v13 沿用） ===
# v9 缺陷：直接 mv 两个临时文件到最终路径，reconcile 在另一个独立代码块
# 中执行——reconcile 失败时错误快照已写入最终路径。
# v10 修订（历史）：snapshot / sha256 / reconcile 全部在 staging 目录内完成；
# v13 修订（per codex v12 P1 #1）：staging 位于**固定事务目录**
# ${OUT_DIR}/.txn/staging（不依赖进程 PID），kill -9 后新进程可从同一路径
# 找回新 tarball / 新 sha256；最终路径提交的崩溃恢复由 .txn/journal
# 结构化状态机保证（kill -9 不执行 trap，故 journal 是唯一可靠恢复机制，
# per codex v11 P1 #1 + codex v12 P1 #1-#4）。
# trap 仅覆盖正常（非 kill -9）失败路径：staging 阶段的普通失败清理 .txn
# 后退出，旧对未动；commit 阶段开始前 `trap - EXIT`（见 4a）。
TXN_DIR="${OUT_DIR}/.txn"
STAGE_DIR="${TXN_DIR}/staging"
STAGE_TAR="${STAGE_DIR}/d-stage-snapshot.tar.zst"
STAGE_SHA="${STAGE_DIR}/d-stage-snapshot.tar.zst.sha256"
STAGE_SCRATCH="${STAGE_DIR}/.scratch"
OLD_TAR="${TXN_DIR}/old.tar.zst"
OLD_SHA="${TXN_DIR}/old.sha256"
JOURNAL="${TXN_DIR}/journal"
trap 'rm -rf -- "$TXN_DIR" "$TMP_TAR" "$TMP_SHA"' EXIT

mkdir -p -- "$STAGE_DIR" "$STAGE_SCRATCH"

# 1. 把临时 tarball / sha256 移到 staging（用最终 basename）
mv -f -- "$TMP_TAR" "$STAGE_TAR"
mv -f -- "$TMP_SHA" "$STAGE_SHA"
trap 'rm -rf -- "$TXN_DIR"' EXIT   # 更新 trap：TMP_TAR/SHA 已移走

# 2. staging 内 sha256sum -c 自校验（确保 .sha256 描述的就是 .tar.zst）
if ! ( cd "$STAGE_DIR" && LC_ALL=C sha256sum -c \
         "d-stage-snapshot.tar.zst.sha256" ) >/dev/null; then
  echo "ERROR: staging 内 sha256sum -c 失败：${STAGE_DIR}/d-stage-snapshot.tar.zst" >&2
  exit 1
fi

# 3. reconcile 在 staging 内执行（per codex v9 P1 #3：必须先 reconcile，
#    reconcile FAIL → 不提交任何最终路径文件）
# reference manifest = 4.0.2-i3 阶段二的 D_stage manifest 输入 lock 2
# v15 per codex v11 P1 #2 + codex v14 P2 #4：三类命名参数（--d-stage-root /
# --out-dir / --reference-manifest）在脚本开头统一真实解析（两种形式
# --reference-manifest=<path> 与 --reference-manifest <path> 均支持；
# 未知参数 fail-closed exit 1）；此处只做默认值回填与 canonicalize +
# existence check。缺失参数时使用默认值
# <out-dir>/../d-stage-audit.D_stage.manifest.tsv（与审计文档 §五接口
# 一致）。任何情况下：先 canonicalize，再校验文件存在且为 regular file；
# 任一失败 → exit 4（reference manifest 缺失或不可访问）。
#
# === reference manifest 默认值 + canonicalize + existence check（v15 per
# codex v14 P2 #4：参数解析已在脚本开头统一完成（三类命名参数真实解析 +
# 未知参数 fail-closed），此处只做默认值回填与路径校验） ===
# 缺失参数策略：--reference-manifest 是**可选**参数（有默认值）；任一来
# 源（CLI / 默认）拿到路径后都必须 canonicalize + existence check；
# 任一失败 → exit 4。严禁"必须显式传递"（v11 错误措辞，v12 删除）。
REFERENCE_MANIFEST_DEFAULT="${OUT_DIR}/../d-stage-audit.D_stage.manifest.tsv"
if [ -z "$REFERENCE_MANIFEST_RAW" ]; then
  # 调用方未传 --reference-manifest：使用默认路径
  REFERENCE_MANIFEST_RAW="$REFERENCE_MANIFEST_DEFAULT"
fi
REFERENCE_MANIFEST=$(realpath -m -- "$REFERENCE_MANIFEST_RAW" 2>/dev/null || echo "")
if [ -z "$REFERENCE_MANIFEST" ] || [ ! -f "$REFERENCE_MANIFEST" ]; then
  echo "ERROR: reference manifest 不存在或不可访问：${REFERENCE_MANIFEST_RAW}" >&2
  echo "  v24 CLI 期望：--reference-manifest <path>（可选）" >&2
  echo "  默认：<out-dir>/../d-stage-audit.D_stage.manifest.tsv" >&2
  echo "  前置条件：必须先完成 gen-manifest (--label D_stage)" >&2
  echo "  调用方可显式传递 --reference-manifest <path> 或依赖默认路径" >&2
  exit 4
fi
echo "INFO: snapshot reference manifest = $REFERENCE_MANIFEST"

# 解压 + 重建 manifest + 4 字段 diff（在 staging 内的 scratch 子目录）
tar --use-compress-program=zstd -xf "$STAGE_TAR" -C "$STAGE_SCRATCH"
EXTRACT_ROOT="${STAGE_SCRATCH}/d-stage"
NEW_MANIFEST="${STAGE_DIR}/.new.manifest.tsv"
DIFF_OUT="${STAGE_DIR}/.diff.out"

( cd "$STAGE_SCRATCH" && \
  find d-stage -mindepth 0 -type d -printf 'd\t%P\t\t\0' ) \
  | LC_ALL=C sort -z > "${STAGE_DIR}/.d.tsv"
( cd "$STAGE_SCRATCH" && \
  find d-stage -mindepth 0 -type l -printf '%P\0' ) \
  | LC_ALL=C sort -z \
  | while IFS= read -r -d '' fpath; do
      target=$(LC_ALL=C readlink "${EXTRACT_ROOT}/${fpath}")
      printf 'l\t%s\t%s\t\0' "$fpath" "$target"
    done > "${STAGE_DIR}/.l.tsv"
( cd "$STAGE_SCRATCH" && \
  find d-stage -mindepth 0 -type f -printf '%P\0' ) \
  | LC_ALL=C sort -z \
  | while IFS= read -r -d '' fpath; do
      sha=$(LC_ALL=C sha256sum "${EXTRACT_ROOT}/${fpath}" \
              | LC_ALL=C awk '{print $1}')
      printf 'f\t%s\t\t%s\0' "$fpath" "$sha"
    done > "${STAGE_DIR}/.f.tsv"

cat -- "${STAGE_DIR}/.d.tsv" "${STAGE_DIR}/.l.tsv" "${STAGE_DIR}/.f.tsv" \
  | tr '\0' '\n' \
  | LC_ALL=C sort -t$'\t' -k1,1 -k2,2 \
  | LC_ALL=C awk -F'\t' '
      NF != 4 { print "ERROR: NF=" NF " at line " NR > "/dev/stderr"; exit 1 }
      { for (i=1;i<=4;i++) if ($i ~ /[\t\r\n\0]/) { print "ERROR: control char field " i " line " NR > "/dev/stderr"; exit 1 }
        print }
    ' > "$NEW_MANIFEST"

if ! LC_ALL=C diff -u "$REFERENCE_MANIFEST" "$NEW_MANIFEST" > "$DIFF_OUT"; then
  cat "$DIFF_OUT" >&2
  echo "ERROR: staging 内 tarball ↔ manifest reconcile 失败（4 字段比对不一致）" >&2
  echo "  → 任何最终路径文件均未提交；请检查 D_stage 源树与 manifest 输入 lock 2" >&2
  exit 1
fi

# 4. reconcile PASS → 提交到最终路径（v15 per codex v14 4 项 finding 重写：
#    v14 协议两处时序/事实缺陷 + 两处接口残留——
#    P1 #1：v14 声称锁在一切动作之前，但草图在临时文件创建、.txn/staging
#           生成、reconcile 之后才加锁（旧 :812）→ 两进程仍可在加锁前
#           共享/覆盖 .txn/staging → v15 把 acquire_snapshot_lock 移到
#           脚本开头（参数解析之后、任何文件系统变更之前）；
#    P1 #2：v14 恢复只校验 journal 计划枚举值与一致性，不校验 .txn/old.*
#           实际文件；回滚 `[ -e ] ... || true` 静默忽略丢失备份 → 可能
#           产生半对 → v15 增加 journal 计划 + .txn/old.* + OUT_DIR 三者
#           fail-closed 事实一致性校验（内容感知区分新旧），回滚段改为
#           内容感知 + mv 失败 exit 9（journal 重入收敛），去掉 || true；
#    P2 #3：audit.md 活动接口仍残留 v12/v13 标签 → v15 统一当前标签；
#    P2 #4：草图只真解析 --reference-manifest，--d-stage-root/--out-dir
#           被当未知参数忽略且脚本仍从 $1/$2 读位置参数 → v15 脚本开头
#           统一真实解析三类命名参数，未知参数 fail-closed exit 1。
#    不变量不变：OUT_DIR 中 tarball 与 .sha256 在任一时刻要么都是新文件，
#    要么都是旧文件；任何 kill -9 / SIGTERM / 断电 / OOM 后，下次启动按
#    journal + 文件系统事实恢复为"新对"或"旧对"，且旧对永不因 journal
#    写入失败或回滚静默忽略而丢失。
#    v19 per codex v18 P1 #1：断电语义由 **fsync 屏障** 保证——每个
#    persist_journal 状态落盘之前，先持久化该状态所依赖的文件与目录
#    （staged ← staging 文件 + staging 目录；backed_up ← .txn 目录；
#    tarball_committed / sha_committed / verified 清理 ← OUT_DIR 目录），
#    否则断电后可能出现"journal=staged 持久但 staging 文件缺失/旧内容"
#    → 下次启动只能 exit 9，无法满足本不变量。
#
# === v15 排他锁 + 持久化事务目录 + 结构化 journal 协议 ===
#
# 布局（.snapshot.lock 在 OUT_DIR；其余全部在 ${OUT_DIR}/.txn 内，固定名，
# 不依赖 PID）：
#   .snapshot.lock                                排他锁文件（永不删除）
#   .txn/staging/d-stage-snapshot.tar.zst        新 tarball（killed 后保留）
#   .txn/staging/d-stage-snapshot.tar.zst.sha256 新 sha256
#   .txn/staging/.scratch/                       reconcile 解压临时区
#   .txn/old.tar.zst                             旧 tarball 备份
#   .txn/old.sha256                              旧 sha256 备份
#   .txn/journal                                 结构化状态（多行 key=value）
#
# === Step 0：OUT_DIR 排他锁（v14 per codex v13 P1 #1） ===
# 实现语言 = python3（O-2 工具本体；锁在进程内持有至退出）：
#   def acquire_snapshot_lock(out_dir: str) -> int:
#       path = os.path.join(out_dir, ".snapshot.lock")
#       fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o644)   # 锁文件永不删除
#       try:
#           fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)  # 非阻塞排他
#       except OSError:
#           os.close(fd)
#           sys.exit(6)                                     # 锁获取失败
#       return fd   # 进程存活期间持有；exit/kill -9 时内核随 fd 关闭释放
# 规则：
#   - **v15 per codex v14 P1 #1：锁在参数解析之后、任何文件系统变更之前
#     获取**（先于临时文件 TMP_TAR/TMP_SHA 创建、.txn 创建、staging 生成、
#     reconcile；v14 草图在 reconcile 之后才加锁，两进程仍可在加锁前
#     共享/覆盖 .txn/staging，v15 修正）；
#   - 锁获取失败 → exit 6 + 明确错误信息，不读 journal、不动任何文件；
#   - 锁覆盖整个运行期直至进程退出；kill -9 / 断电后 flock 由内核释放，
#     下一次 snapshot 可正常获取；
#   - .snapshot.lock 为永久文件，任何阶段都**不得删除**（删除会破坏
#     不同进程间的锁互斥——unlink 后新进程会创建新 inode 重新加锁）；
#   - bash 草图等价：`acquire_snapshot_lock "${OUT_DIR}"`（若用 bash 实现
#     且无 util-linux flock，须以 python3 子进程持锁并将 flock 1.x 写入
#     genesis.json tool_versions；本框架主路径 = python3 工具，不引入
#     bash 持锁变体）。
#
# journal 结构化格式（每行一个 key=value；schema 固定 1；缺 schema 或
# 缺 state/staging_dir 视为损坏；字段取值范围见恢复检查的严格校验）：
#   schema=1
#   state=<staged|backed_up|tarball_committed|sha_committed|verified|rolling_back>
#   staging_dir=<absolute path>       # = ${OUT_DIR}/.txn/staging（恢复时重新绑定）
#   new_tar_sha256=<64 hex>           # 新 tarball SHA-256（staged 起记录）
#   new_tar_size=<bytes>              # 新 tarball 字节数（staged 起记录）
#   old_tar=<present|absent>          # 备份计划：旧 tarball 是否存在
#   old_sha=<present|absent>          # 备份计划：旧 sha256 是否存在
#   old_tar_sha256=<64 hex|none>      # 旧 tarball 的 SHA-256 指纹（v16 per
#                                     # codex v15 P1 #1；plan=absent 时为 none）
#   old_sha_sha256=<64 hex|none>      # 旧 .sha256 文件本身的 SHA-256 指纹
#                                     # （v16 per codex v15 P1 #1；plan=absent 时为 none）
#
# journal 持久化（per codex v12 P1 #4 显式 syscall 路径；实现语言 =
# python3，O-2 工具 `tools/d-stage-audit-gen.py` 内逐字实现；bash 草图
# 通过同名 persist_journal 函数委托同一序列；**严禁**以全局 `sync`(1)
# 替代——sync 不保证目标文件 fsync 也不做目录 fsync）：
#   def persist_journal(path, fields):
#       tmp = path + ".tmp"
#       with open(tmp, "w") as f:
#           f.write("\n".join(f"{k}={v}" for k, v in fields.items()) + "\n")
#           f.flush()
#           os.fsync(f.fileno())          # ① 文件数据+元数据落盘
#       os.rename(tmp, path)              # ② atomic replace（失败=旧 journal 保留）
#       dfd = os.open(os.path.dirname(path) or ".", os.O_RDONLY | os.O_DIRECTORY)
#       try:
#           os.fsync(dfd)                 # ③ 目录项落盘（rename 持久化）
#       finally:
#           os.close(dfd)
#   失败语义：①或②或③任一抛异常 → journal 仍是旧内容；调用方按当前
#   Stage 的失败分支处理。持久化顺序不可交换：文件 fsync 必须先于 rename，
#   目录 fsync 必须在 rename 之后（否则崩溃后 rename 可能丢失）。
#
# v19 per codex v18 P1 #1：新增 fsync_file / fsync_dir 两个 helper（同一
# python3 显式 syscall 路径），构成**状态依赖数据屏障**——每个 journal
# 状态落盘前必须先持久化该状态所依赖的文件与目录：
#   def fsync_file(path):                  # 文件数据+元数据落盘
#       fd = os.open(path, os.O_RDONLY); os.fsync(fd); os.close(fd)
#   def fsync_dir(path):                   # 目录项落盘（os.O_DIRECTORY）
#       dfd = os.open(path, os.O_RDONLY | os.O_DIRECTORY); os.fsync(dfd); os.close(dfd)
#   屏障顺序（不可交换；任一失败 → exit 5，journal 保持前一持久状态）：
#     persist(staged)            ← fsync_file(staging tar) + fsync_file(staging sha)
#                                  + fsync_dir(staging dir)（先于任何旧文件移动）
#     persist(backed_up)         ← 备份 mv 后 fsync_dir(.txn) + fsync_dir(OUT_DIR)
#                                  （v20 per codex v19 P1 #1：跨目录 rename 源、
#                                  目标目录都同步；只同步目标目录会在断电后残留
#                                  OUT_DIR 源目录项 → 旧文件双存在 → 恢复 exit 9）
#     persist(tarball_committed) ← 新 tar mv 到 OUT_DIR 后 fsync_dir(OUT_DIR)
#                                  （staging 源目录残留项由清理 rm -rf .txn 消除，
#                                  不影响恢复判定，无需单独同步）
#     persist(sha_committed)     ← 新 sha mv 到 OUT_DIR 后 fsync_dir(OUT_DIR)（同上）
#     persist(verified)          ← sha256sum -c PASS（文件本身已由 fsync 持久）
#     清理完成                   ← rm old.* 后 fsync_dir(.txn)（v21 per codex
#                                  v20 P1 #3，先于 rm journal）→ rm journal /
#                                  rm -rf .txn → fsync_dir(OUT_DIR)
#     回滚段                     ← **每个**还原 mv 后立即 fsync_dir(OUT_DIR) +
#                                  fsync_dir(.txn)（v20 per codex v19 P1 #1 源、
#                                  目标都同步 + v23 per codex v22 P1 #1 逐次 mv
#                                  屏障：两次还原 mv 之间断电时，统一 fsync 会
#                                  导致第一次 mv 的源/目标目录项未持久 → 旧 tar
#                                  双存在或均缺失 exit 9；逐次屏障后 (old,absent)
#                                  由持久化保证；v24 per codex v23 P1 #1 任一
#                                  fsync 失败 exit 5 统一；P2 #2 还原 mv 后任一
#                                  目录 fsync 未完成断电 → 一致可重入 / 双存在 /
#                                  均缺失 → 三者事实校验 exit 9 保留证据），
#                                  清理后再次 fsync_dir(OUT_DIR)
#
# 状态机（journal 每次写入都发生在"下一步动作"之前，保证写失败时旧
# 状态仍持久、动作尚未发生）：
#   staged              staging 新文件已生成 + 自校验 + reconcile PASS；
#                       journal 已含指纹与备份计划；**尚未移动任何旧文件**
#   backed_up           旧对已移入 .txn/old.*
#   tarball_committed   新 tarball 已到 OUT_DIR
#   sha_committed       新 sha256 已到 OUT_DIR
#   verified            OUT_DIR 最终 sha256sum -c PASS
#   rolling_back        自校验 FAIL；回滚意图已落盘（在 rm 新文件之前写）
#   清理                删 .txn/old.* → 删 journal → rm -rf .txn（无独立状态；
#                       verified / rolling_back 或事实推断驱动）
#
# 恢复算法（每次 snapshot 启动执行；幂等；**前置 = Step 0 排他锁已获取**，
# 锁获取失败直接 exit 6，不进入任何恢复/提交动作；journal 状态是"至少已
# 完成"的下界，实际 resume 点用文件系统事实修正）：
#   facts = { txn_exists, journal_parses, state,
#             old_tar_in_txn, old_sha_in_txn,
#             new_tar_in_out, new_sha_in_out,
#             staging_tar_in_txn, staging_sha_in_txn }
#   A. .txn 不存在 → 全新路径。
#   B. .txn 存在但 journal 缺失：v13 顺序不变式保证"旧文件进入 .txn 之前
#      journal(staged) 必已落盘"，因此合法窗口只有两个：(a) staged 落盘前
#      崩溃（旧对未动，staging 残缺）；(b) 清理阶段 journal 已删、.txn 未删
#      （OUT_DIR 新对完整）。恢复：若 .txn/old.tar.zst 或 old.sha256 存在
#      → 不变式违反（人为干预）→ exit 9；否则 rm -rf .txn，按全新路径继续
#      （v21 per codex v20 P1 #2：不再因"OUT_DIR 自洽"直接 exit 0——无
#      journal 即无新对指纹证据，旧对可能被误判为新对；"上次已完成"情形
#      由 4a 幂等 no-op 在严格校验下等价收敛）。
#   C. .txn + journal 存在但不可解析（schema≠1 / state 缺失 / 未知 state /
#      字段取值范围非法 / old_tar 与 old_sha 计划不一致）：
#      → exit 9（dsh 人工清理 .txn + OUT_DIR 半新文件）。
#   D. 可解析 → 先做 **staging_dir 路径约束**（v14 per codex v13 P1 #2）：
#        1) 必须绝对路径；2) realpath -m canonicalize（消除 .. / // / .）；
#        3) canonical 结果必须 is_within canonical(.txn/staging)；
#        4) 路径本身不得是 symlink（-L 拒绝）；5) 必须存在且为目录；
#       不满足任一 → exit 9。
#   E. **三者事实一致性校验**（v16 per codex v14 P1 #2 + codex v15 P1 #1）：
#      journal 计划 + .txn/old.* 事实 + OUT_DIR 事实 fail-closed，且
#      **只有"已知旧对"或"已知新对"可接受**：
#      - OUT_DIR tarball 分类：不存在 = absent；sha256 == new_tar_sha256 →
#        new；sha256 == old_tar_sha256 → old；其余一律 unknown → exit 9
#        （损坏 / 第三方替换 / 未知内容不得误判为旧文件）；
#      - OUT_DIR .sha256 分类：先做**精确格式校验**（单行
#        `^[0-9a-f]{64}  d-stage-snapshot\.tar\.zst$`，否则 exit 9），再取
#        hash 字段：== new_tar_sha256 → new；== old_tar_sha256 → old；
#        其余 unknown → exit 9；
#      - .txn/old.tar.zst 存在时必须 sha256 == old_tar_sha256（否则 exit 9）；
#        .txn/old.sha256 存在时必须自身 sha256 == old_sha_sha256 且格式与
#        hash 字段合法（否则 exit 9）；
#      - 计划 present → 旧文件必须恰好存在于 .txn 或 OUT_DIR 之一（两者
#        同时存在 / 均不存在 → exit 9，绝不静默恢复成半对）；OUT_DIR 出现
#        old 状态文件时另一文件也必须同为 old 或 absent（混合 new/old →
#        exit 9）；计划 absent → .txn 内不得存在旧文件、OUT_DIR 不得出现
#        old 状态文件。随后按 state 进入对应步骤（每步先查 facts 再行动，
#        mv 带存在性守卫，重复执行幂等）：
#        staged            → 校验 staging_dir 重新绑定 + 新文件存在且
#                            sha256/size 与 journal 一致（不一致 exit 9）
#                            → 补做备份 mv（守卫）→ persist backed_up
#        backed_up         → 新 tarball 不在 OUT_DIR 则 mv（守卫）→
#                            persist tarball_committed → 新 sha256 不在
#                            OUT_DIR 则 mv（守卫）→ persist sha_committed
#        tarball_committed → 同 backed_up 从 sha256 提交开始
#        sha_committed     → 最终自校验：PASS → persist verified → 清理；
#                            FAIL → persist rolling_back → 回滚段
#        verified          → 清理（rm old → rm journal → rm -rf .txn）
#        rolling_back      → 回滚段（rm OUT_DIR new 状态文件[守卫] → mv
#                            old 回 OUT_DIR[守卫] → rm journal → rm -rf .txn）
#   回滚段/清理段中途崩溃 → journal 仍为 rolling_back / verified → 下次
#   启动重入（守卫幂等）。最终 OUT_DIR = 旧对（rolling_back）或新对
#   （verified + 清理完成）。
#   全新路径 4a 之前同样强制**旧对完整性 fail-closed**（v14 per codex v13
#   P2 #3）：OUT_DIR 既有 tarball 与 .sha256 必须"两者都在或都不在"，
#   半对 → exit 9，不写 journal、不动文件。
#   注意：Step 0 排他锁已在脚本开头（参数解析之后、任何文件系统变更之前）
#   获取（v15 per codex v14 P1 #1），此处不再重复加锁。

# ================ 共享分类函数（v24 per codex v16 P1 #1 + codex v17 P1 #1 + codex v18 P2 #3） ================
# 即时回滚（4e FAIL 分支）与启动恢复**必须共用同一分类函数**，禁止各自
# 实现不一致的"内容感知"逻辑。副作用：OUT_TAR_STATE / OUT_SHA_STATE ∈
# {absent, new, old, unknown}。sidecar 判定必须同时满足：恰好 1 行（wc -l
# == 1，grep -qEx 只证明存在一条匹配行、不保证文件只有一行，v17 修正）+
# 精确格式 `^[0-9a-f]{64}  d-stage-snapshot\.tar\.zst$` + hash 字段 ∈
# {新指纹, 旧指纹}；任一不满足 → unknown。
# v18 per codex v17 P1 #1 + v19 per codex v18 P2 #3：旧、新指纹相等的歧义
# 场景（确定性重跑）已在 4a 以幂等 no-op 前置处理（既有对 == 新对 → 直接
# 成功返回，不写 journal），因此恢复路径中不会出现"旧对内容恰等于新快照"
# 的分类歧义。no-op 判定复用 4a 严格 sidecar 校验结论（恰好一行 + 精确
# 格式 + hash 字段），malformed/duplicate sidecar 先 exit 9、绝不进入
# no-op（裸 sha256sum -c 会放行两行重复合法行，已弃用，per codex v18 P2 #3）。
classify_out_pair() {
  OUT_TAR_STATE=absent; OUT_SHA_STATE=absent
  if [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst" ]; then
    local s
    s=$(LC_ALL=C sha256sum "${OUT_DIR}/d-stage-snapshot.tar.zst" | LC_ALL=C awk '{print $1}')
    if [ "$s" = "${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" ]; then OUT_TAR_STATE=new
    elif [ "$s" = "${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" ]; then OUT_TAR_STATE=old
    else OUT_TAR_STATE=unknown; fi
  fi
  if [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" ]; then
    local n h
    n=$(LC_ALL=C wc -l < "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" | LC_ALL=C tr -d ' ')
    if [ "$n" != "1" ] \
       || ! LC_ALL=C grep -qEx '[0-9a-f]{64}  d-stage-snapshot\.tar\.zst' \
            "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256"; then
      OUT_SHA_STATE=unknown
    else
      h=$(LC_ALL=C awk '{print $1}' "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256")
      if [ "$h" = "${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" ]; then OUT_SHA_STATE=new
      elif [ "$h" = "${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" ]; then OUT_SHA_STATE=old
      else OUT_SHA_STATE=unknown; fi
    fi
  fi
}

# ================= 恢复检查（v17；per codex v12 P1 #2 结构化解析 + codex v13 P1 #2 路径约束 + codex v14 P1 #2 事实一致性 + codex v15 P1 #1 指纹严格匹配 + codex v16 P1 #1+#2） =================
RECOVERED=0
if [ -d "$TXN_DIR" ]; then
  # v14 per codex v13 P1 #2：.txn 本身不得是 symlink（否则 canonical 校验
  # 与守卫 mv 可能落到事务目录之外）
  if [ -L "$TXN_DIR" ]; then
    echo "ERROR: .txn 为 symlink（拒绝恢复）→ exit 9" >&2
    exit 9
  fi
  if [ -f "$JOURNAL" ]; then
    SCHEMA_J=$(sed -n 's/^schema=//p' "$JOURNAL" | tail -1)
    STATE=$(sed -n 's/^state=//p' "$JOURNAL" | tail -1)
    STAGING_DIR_J=$(sed -n 's/^staging_dir=//p' "$JOURNAL" | tail -1)
    NEW_TAR_SHA_J=$(sed -n 's/^new_tar_sha256=//p' "$JOURNAL" | tail -1)
    NEW_TAR_SIZE_J=$(sed -n 's/^new_tar_size=//p' "$JOURNAL" | tail -1)
    OLD_TAR_PLAN=$(sed -n 's/^old_tar=//p' "$JOURNAL" | tail -1)
    OLD_SHA_PLAN=$(sed -n 's/^old_sha=//p' "$JOURNAL" | tail -1)
    OLD_TAR_SHA_J=$(sed -n 's/^old_tar_sha256=//p' "$JOURNAL" | tail -1)
    OLD_SHA_SHA_J=$(sed -n 's/^old_sha_sha256=//p' "$JOURNAL" | tail -1)
    if [ "$SCHEMA_J" != "1" ] || [ -z "$STATE" ] || [ -z "$STAGING_DIR_J" ]; then
      echo "ERROR: journal 不可解析（schema=${SCHEMA_J:-<missing>} state=${STATE:-<missing>}）→ exit 9" >&2
      echo "  请 dsh 人工清理 ${TXN_DIR} + OUT_DIR 内半新文件后再重跑" >&2
      exit 9
    fi
    # 字段严格校验（v16 per codex v13 P1 #2 + codex v15 P1 #1：拒绝"格式合法
    # 但取值非法"；旧对指纹为 64 hex 或 none）
    case "$OLD_TAR_PLAN" in present|absent) ;; *) echo "ERROR: journal old_tar 取值非法：${OLD_TAR_PLAN} → exit 9" >&2; exit 9 ;; esac
    case "$OLD_SHA_PLAN" in present|absent) ;; *) echo "ERROR: journal old_sha 取值非法：${OLD_SHA_PLAN} → exit 9" >&2; exit 9 ;; esac
    if [ "$OLD_TAR_PLAN" != "$OLD_SHA_PLAN" ]; then
      echo "ERROR: journal old_tar/old_sha 计划不一致（present/absent 混合）→ exit 9" >&2
      exit 9
    fi
    printf '%s' "$NEW_TAR_SHA_J" | LC_ALL=C grep -qE '^[0-9a-f]{64}$' || {
      echo "ERROR: journal new_tar_sha256 格式非法 → exit 9" >&2; exit 9; }
    printf '%s' "$NEW_TAR_SIZE_J" | LC_ALL=C grep -qE '^[0-9]+$' || {
      echo "ERROR: journal new_tar_size 格式非法 → exit 9" >&2; exit 9; }
    printf '%s' "$OLD_TAR_SHA_J" | LC_ALL=C grep -qE '^([0-9a-f]{64}|none)$' || {
      echo "ERROR: journal old_tar_sha256 格式非法 → exit 9" >&2; exit 9; }
    printf '%s' "$OLD_SHA_SHA_J" | LC_ALL=C grep -qE '^([0-9a-f]{64}|none)$' || {
      echo "ERROR: journal old_sha_sha256 格式非法 → exit 9" >&2; exit 9; }
    # v16 per codex v14 P1 #2 + codex v15 P1 #1：三者事实一致性 fail-closed，
    # 只有"已知旧对"或"已知新对"可接受（未知/损坏/第三方替换一律 exit 9）。
    # OUT_DIR 分类：absent / new / old / unknown；.txn 内容必须匹配指纹。
    OUT_TAR_STATE=absent; OUT_SHA_STATE=absent
    OLD_TAR_IN_TXN=0; OLD_SHA_IN_TXN=0
    if [ -e "$OLD_TAR" ]; then
      OLD_TAR_IN_TXN=1
      TXN_OLD_TAR_SHA=$(LC_ALL=C sha256sum "$OLD_TAR" | LC_ALL=C awk '{print $1}')
      [ "$TXN_OLD_TAR_SHA" = "$OLD_TAR_SHA_J" ] || {
        echo "ERROR: .txn/old.tar.zst 内容与 journal old_tar_sha256 指纹不符 → exit 9" >&2; exit 9; }
    fi
    if [ -e "$OLD_SHA" ]; then
      OLD_SHA_IN_TXN=1
      TXN_OLD_SHA_SHA=$(LC_ALL=C sha256sum "$OLD_SHA" | LC_ALL=C awk '{print $1}')
      [ "$TXN_OLD_SHA_SHA" = "$OLD_SHA_SHA_J" ] || {
        echo "ERROR: .txn/old.sha256 内容与 journal old_sha_sha256 指纹不符 → exit 9" >&2; exit 9; }
      { [ "$(LC_ALL=C wc -l < "$OLD_SHA" | LC_ALL=C tr -d ' ')" = "1" ] \
        && LC_ALL=C grep -qEx '[0-9a-f]{64}  d-stage-snapshot\.tar\.zst' "$OLD_SHA"; } || {
        echo "ERROR: .txn/old.sha256 格式非法（非恰好一行 '<64hex>  d-stage-snapshot.tar.zst'）→ exit 9" >&2; exit 9; }
    fi
    # v17 per codex v16 P1 #1：调用共享分类函数（即时回滚与启动恢复共用）
    classify_out_pair
    if [ "$OUT_TAR_STATE" = "unknown" ]; then
      echo "ERROR: OUT_DIR tarball 内容未知（既非新快照指纹也非旧快照指纹，疑似损坏/替换）→ exit 9" >&2
      exit 9
    fi
    if [ "$OUT_SHA_STATE" = "unknown" ]; then
      echo "ERROR: OUT_DIR .sha256 内容未知（非单行合法格式或 hash 字段既非新快照也非旧快照）→ exit 9" >&2
      exit 9
    fi
    # OUT_DIR 新旧不得混合（同为 new / 同为 old / 其一 absent 均合法；混合 → exit 9）。
    # v21 per codex v20 P1 #1：该混合判定**仅对非 rolling_back 状态生效**；
    # rolling_back 逐文件删除/还原的中途事实态（(new,new)/(absent,new)/
    # (absent,absent)/(old,absent)/(old,old)）由下方 rolling_back 白名单
    # 显式允许——否则断电停在删除第一个新文件之后（absent/new）时，下次
    # 启动会在任何"统一混合判定"处被拒，永远到不了 rolling_back 重入段。
    if [ "$STATE" != "rolling_back" ]; then
      if [ "$OUT_TAR_STATE" != "absent" ] && [ "$OUT_SHA_STATE" != "absent" ] \
         && [ "$OUT_TAR_STATE" != "$OUT_SHA_STATE" ]; then
        echo "ERROR: OUT_DIR tarball 与 .sha256 新旧状态混合（${OUT_TAR_STATE} vs ${OUT_SHA_STATE}）→ exit 9" >&2
        exit 9
      fi
    fi
    # 计划 present：旧文件必须恰好存在于 .txn 或 OUT_DIR 之一
    if [ "$OLD_TAR_PLAN" = "present" ]; then
      if [ "$OLD_TAR_IN_TXN" = "1" ] && [ "$OUT_TAR_STATE" = "old" ]; then
        echo "ERROR: 旧 tarball 同时存在于 .txn 与 OUT_DIR（状态不一致，疑似人为干预）→ exit 9" >&2; exit 9
      fi
      if [ "$OLD_TAR_IN_TXN" = "0" ] && [ "$OUT_TAR_STATE" != "old" ]; then
        echo "ERROR: journal old_tar=present 但旧 tarball 在 .txn 与 OUT_DIR 均不可证明（备份丢失或内容未知，拒绝静默恢复成半对）→ exit 9" >&2; exit 9
      fi
      if [ "$OLD_SHA_IN_TXN" = "1" ] && [ "$OUT_SHA_STATE" = "old" ]; then
        echo "ERROR: 旧 sha256 同时存在于 .txn 与 OUT_DIR（状态不一致，疑似人为干预）→ exit 9" >&2; exit 9
      fi
      if [ "$OLD_SHA_IN_TXN" = "0" ] && [ "$OUT_SHA_STATE" != "old" ]; then
        echo "ERROR: journal old_sha=present 但旧 sha256 在 .txn 与 OUT_DIR 均不可证明（备份丢失或内容未知，拒绝静默恢复成半对）→ exit 9" >&2; exit 9
      fi
    else
      # 计划 absent：.txn 内不得存在旧文件；OUT_DIR 不得出现 old 状态文件（不变式）
      [ "$OLD_TAR_IN_TXN" = "0" ] || { echo "ERROR: journal old_tar=absent 但 .txn/old.tar.zst 存在（不变式违反）→ exit 9" >&2; exit 9; }
      [ "$OLD_SHA_IN_TXN" = "0" ] || { echo "ERROR: journal old_sha=absent 但 .txn/old.sha256 存在（不变式违反）→ exit 9" >&2; exit 9; }
      [ "$OUT_TAR_STATE" != "old" ] || { echo "ERROR: journal old_tar=absent 但 OUT_DIR tarball 为旧快照内容（不变式违反）→ exit 9" >&2; exit 9; }
      [ "$OUT_SHA_STATE" != "old" ] || { echo "ERROR: journal old_sha=absent 但 OUT_DIR .sha256 为旧快照内容（不变式违反）→ exit 9" >&2; exit 9; }
    fi
    # v17 per codex v16 P1 #2：状态专属不变量（fail-closed）。verified 必须
    # 要求 OUT_DIR 两个文件均为 new（完整新对）；半对/未知 → exit 9 且**不
    # 清理任何证据**。staged / backed_up / tarball_committed / rolling_back
    # 分别允许其明确的中间态（含 mv 与 journal 写之间的崩溃窗口）。
    case "$STATE" in
      staged)
        # 旧对尚未移动（或本就不存在）；OUT_DIR 不得出现 new 状态文件
        [ "$OUT_TAR_STATE" != "new" ] || { echo "ERROR: journal=staged 但 OUT_DIR tarball 为新快照内容（状态不一致）→ exit 9" >&2; exit 9; }
        [ "$OUT_SHA_STATE" != "new" ] || { echo "ERROR: journal=staged 但 OUT_DIR .sha256 为新快照内容（状态不一致）→ exit 9" >&2; exit 9; }
        ;;
      backed_up)
        # 旧对已入 .txn；新 tar 可能已提交（4c mv 与 journal 写之间崩溃）；
        # 新 sha 不可能出现（4d 在 tarball_committed 之后）
        [ "$OUT_SHA_STATE" = "absent" ] || { echo "ERROR: journal=backed_up 但 OUT_DIR .sha256 存在（状态不一致）→ exit 9" >&2; exit 9; }
        ;;
      tarball_committed)
        [ "$OUT_TAR_STATE" = "new" ] || { echo "ERROR: journal=tarball_committed 但 OUT_DIR tarball 非新快照内容（状态不一致）→ exit 9" >&2; exit 9; }
        ;;
      sha_committed|verified)
        [ "$OUT_TAR_STATE" = "new" ] || { echo "ERROR: journal=$STATE 但 OUT_DIR tarball 非新快照内容（${OUT_TAR_STATE}）→ exit 9（不清理证据）" >&2; exit 9; }
        [ "$OUT_SHA_STATE" = "new" ] || { echo "ERROR: journal=$STATE 但 OUT_DIR .sha256 非新快照内容（${OUT_SHA_STATE}）→ exit 9（不清理证据）" >&2; exit 9; }
        ;;
      rolling_back)
        # v21 per codex v20 P1 #1：回滚逐文件执行（先删新 tar、再删新
        # sidecar、再逐个还原旧 tar/旧 sidecar），断电可停在任何一步之间。
        # 合法中途事实态（OUT_DIR tar|sidecar 状态对）显式白名单：
        #   new|new       回滚尚未开始删除（重入后从头删起，守卫幂等）
        #   absent|new    已删新 tar、未删新 sidecar
        #   absent|absent 新文件均已删除、旧文件尚未还原
        #   old|absent    已还原旧 tar、未还原旧 sidecar
        #   old|old       旧对已完整还原、清理尚未完成
        # 其余组合（new|old / old|new / absent|old——还原顺序先 tar 后
        # sidecar，不会出现 sidecar 已还原而 tar 未还原）→ exit 9 保留
        # 证据；unknown 已在上方事实块 exit 9。
        # v22 per codex v21 P1 #1：shell case 中未转义的 | 是**模式分隔符**
        # 而非状态对字符串的一部分——`new|new|...` 匹配的是单个 token
        # （new / absent / old），五个白名单状态对实际全部落入拒绝分支。
        # 此处改用**带引号的精确字符串匹配**（引号内 | 为字面量）；
        # 真实实现（Python 契约）用集合成员判断，禁止照抄 shell case
        # 语法（见 030-d-stage-audit.md §五 v24 Python 实现契约）。
        case "${OUT_TAR_STATE}|${OUT_SHA_STATE}" in
          "new|new"|"absent|new"|"absent|absent"|"old|absent"|"old|old") : ;;
          *) echo "ERROR: journal=rolling_back 但 OUT_DIR 状态对（${OUT_TAR_STATE}|${OUT_SHA_STATE}）不在合法中途态白名单 → exit 9（不清理证据）" >&2; exit 9 ;;
        esac
        ;;
    esac
    # staging_dir 路径约束（v14 per codex v13 P1 #2）
    STAGING_DIR_CANON=$(realpath -m -- "$STAGING_DIR_J" 2>/dev/null || echo "")
    TXN_STAGING_CANON=$(realpath -m -- "${TXN_DIR}/staging" 2>/dev/null || echo "")
    case "$STAGING_DIR_J" in
      /*) ;;
      *) echo "ERROR: journal staging_dir 非绝对路径：${STAGING_DIR_J} → exit 9" >&2; exit 9 ;;
    esac
    case "$STAGING_DIR_CANON" in
      "$TXN_STAGING_CANON"|"$TXN_STAGING_CANON"/*) : ;;
      *) echo "ERROR: journal staging_dir 越出 .txn/staging（路径穿越拒绝）：${STAGING_DIR_J} → exit 9" >&2; exit 9 ;;
    esac
    if [ -L "$STAGING_DIR_J" ] || [ ! -d "$STAGING_DIR_J" ]; then
      echo "ERROR: journal staging_dir 为 symlink 或非目录：${STAGING_DIR_J} → exit 9" >&2
      exit 9
    fi
    case "$STATE" in
      staged|backed_up|tarball_committed|sha_committed|verified|rolling_back)
        # journal 记录的 staging_dir 经路径约束校验后重新绑定（per codex v12 P1 #1 + codex v13 P1 #2）
        STAGE_DIR="$STAGING_DIR_J"
        STAGE_TAR="${STAGE_DIR}/d-stage-snapshot.tar.zst"
        STAGE_SHA="${STAGE_DIR}/d-stage-snapshot.tar.zst.sha256"
        RECOVERED=1
        ;;
      *)
        echo "ERROR: journal state 无法识别：${STATE} → exit 9" >&2
        exit 9
        ;;
    esac
  else
    # journal 缺失：不变式校验（旧文件不得在无 journal 的情况下进入 .txn）
    if [ -e "$OLD_TAR" ] || [ -e "$OLD_SHA" ]; then
      echo "ERROR: .txn 内存在 old.* 但 journal 缺失（不变式违反，疑似人为干预）→ exit 9" >&2
      exit 9
    fi
    rm -rf -- "$TXN_DIR"
    # v21 per codex v20 P1 #2：不再把"OUT_DIR 自洽对"直接判为已完成新对并
    # exit 0——无 journal 即无新快照指纹证据，无法区分"上次已提交的新对"
    # 与"从未进入事务的旧对"（power_loss_before_staged_persist 场景旧对
    # 完整 + staging 残留时，旧路径会误判为新对直接成功，而不是按要求
    # 丢弃残留并重新执行 snapshot）。统一丢弃 .txn 残留后**继续全新路径**：
    # 4a 严格 sidecar 校验 + 幂等 no-op 负责"既有对 == 新快照"的成功返回
    # （严格校验，不依赖裸 sha256sum -c）；否则正常进入事务。
    # 注：上一次已完成（verified 清理后 .txn 目录项未持久）的情形同样落入
    # 全新路径：既有新对与新快照内容相同 → 幂等 no-op exit 0，语义不变。
  fi
fi

if [ "$RECOVERED" = "1" ] && [ -e "$STAGE_TAR" ]; then
  # 恢复时校验 staging 新文件指纹（per codex v12 P1 #1：校验并重新绑定）
  ACT_SHA=$(LC_ALL=C sha256sum "$STAGE_TAR" 2>/dev/null | LC_ALL=C awk '{print $1}')
  ACT_SIZE=$(stat -c %s "$STAGE_TAR" 2>/dev/null || echo "")
  if [ "$ACT_SHA" != "$NEW_TAR_SHA_J" ] || [ "$ACT_SIZE" != "$NEW_TAR_SIZE_J" ]; then
    echo "ERROR: staging 新 tarball 指纹与 journal 不一致（恢复拒绝）→ exit 9" >&2
    exit 9
  fi
fi

if [ "$RECOVERED" = "1" ]; then
  case "$STATE" in
    verified)
      # 上次已自校验 PASS：直接进入清理段（守卫幂等）。
      # v17 per codex v16 P1 #2：verified 状态专属不变量已在上方事实块
      # 强制——OUT_DIR 两个文件必须均为 new（完整新对）；半对/未知已在
      # 事实块 exit 9，**不会**走到此处清理证据。
      rm -f -- "$OLD_TAR" "$OLD_SHA"
      # v21 per codex v20 P1 #3：rm old.* 后先 fsync_dir(.txn) 再删 journal
      # （与 4e 清理同一顺序，防"journal 删除已持久但 old.* 未持久"窗口）。
      fsync_dir "$TXN_DIR" || {
        echo "ERROR: .txn 目录 fsync 失败（journal 仍为 verified；下次启动重做清理，幂等）→ exit 5" >&2
        exit 5
      }
      rm -f -- "$JOURNAL"
      rm -rf -- "$TXN_DIR"
      # v19 per codex v18 P1 #1：清理后 fsync OUT_DIR 目录（journal/.txn
      # 目录项删除持久化；失败不影响"新对已完整"，重跑幂等）。
      fsync_dir "$OUT_DIR" || {
        echo "ERROR: OUT_DIR 目录 fsync 失败（清理已执行；重跑幂等）→ exit 5" >&2
        exit 5
      }
      echo "INFO: 恢复完成（上次已 verified）；OUT_DIR = 新对"
      exit 0
      ;;
    rolling_back)
      # 上次已进入回滚：直接重入回滚段（守卫幂等；不得落入提交段）。
      # v16 per codex v14 P1 #2 + codex v15 P1 #1：基于上方事实块的严格
      # 状态分类（absent/new/old；unknown 已在事实块 exit 9）——只删除
      # new 状态文件；old 状态文件（已还原的旧对）原样保留；.txn 还原 mv
      # 失败 → exit 9（journal 保持 rolling_back，下次启动重入收敛），
      # 绝不静默产生半对。
      [ "$OUT_TAR_STATE" = "new" ] && rm -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst"
      [ "$OUT_SHA_STATE" = "new" ] && rm -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256"
      # v23 per codex v22 P1 #1：**每个**跨目录还原 mv 后立即 fsync 源、目标
      # 两个目录，再处理下一个文件——两次还原 mv 之间断电时，若只在全部
      # mv 完成后统一 fsync，第一次 mv 的源/目标目录项可能未持久 → 旧 tar
      # 双存在或均缺失 → 三者事实校验 exit 9，无法到达 (old,absent) 的
      # 确定性自动恢复。逐次屏障后，(old,absent) 由持久化保证。
      # v24 per codex v23 P1 #1：本段 fsync_dir 失败统一 exit 5（与 §五
      # Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）；
      # v24 per codex v23 P2 #2：还原 mv 后、任一目录 fsync 未完成时断电 →
      # 按被还原文件分列四态矩阵：旧 tar → target-only 白名单 (old,absent) /
      # source-only (absent,absent) 且 .txn 有 old.*；旧 sidecar（旧 tar 已持久
      # 还原）→ target-only 白名单 (old,old) / source-only (old,absent) 且 .txn
      # 有 old.*；一致事实均白名单显式允许重入收敛；旧文件双存在 / 均缺失 →
      # 三者事实校验 exit 9 保留证据（不得仅凭 OUT 状态对恰为合法中途态而
      # 错误接受 sidecar 丢失；见故障注入表 v24 新场景，与备份方向 mv-fsync
      # 窗口同一契约）。
      if [ -e "$OLD_TAR" ]; then
        mv -f -- "$OLD_TAR" "${OUT_DIR}/d-stage-snapshot.tar.zst" || {
          echo "ERROR: 回滚还原旧 tarball 失败 → exit 9（journal 保持 rolling_back，下次启动重入）" >&2
          exit 9
        }
        fsync_dir "$OUT_DIR" || {
          echo "ERROR: OUT_DIR 目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
          exit 5
        }
        fsync_dir "$TXN_DIR" || {
          echo "ERROR: .txn 源目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
          exit 5
        }
      fi
      if [ -e "$OLD_SHA" ]; then
        mv -f -- "$OLD_SHA" "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" || {
          echo "ERROR: 回滚还原旧 sha256 失败 → exit 9（journal 保持 rolling_back，下次启动重入）" >&2
          exit 9
        }
        fsync_dir "$OUT_DIR" || {
          echo "ERROR: OUT_DIR 目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
          exit 5
        }
        fsync_dir "$TXN_DIR" || {
          echo "ERROR: .txn 源目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
          exit 5
        }
      fi
      # 最终校验：旧对若存在必须自洽（sha256sum -c 通过），否则 exit 9
      # （内容未知/不匹配绝不静默报告"完整旧对"，per codex v15 P1 #1）
      if [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst" ] || [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" ]; then
        ( cd "${OUT_DIR}" && LC_ALL=C sha256sum -c \
            "d-stage-snapshot.tar.zst.sha256" ) >/dev/null || {
          echo "ERROR: 回滚后 OUT_DIR 旧对自校验失败（内容不匹配）→ exit 9" >&2
          exit 9
        }
      fi
      rm -f -- "$JOURNAL"
      rm -rf -- "$TXN_DIR"
      fsync_dir "$OUT_DIR" || {
        echo "ERROR: OUT_DIR 目录 fsync 失败（旧对已还原；重跑幂等）→ exit 5" >&2
        exit 5
      }
      echo "INFO: 恢复完成（上次已 rolling_back）；OUT_DIR = 完整旧对"
      exit 1
      ;;
    staged|backed_up|tarball_committed|sha_committed)
      ;;   # 落入下方统一提交段（4b-4e，守卫幂等）
  esac
fi

if [ "$RECOVERED" != "1" ]; then
  # ============ 全新路径：4a. 旧对完整性 fail-closed + 持久化 staged（先于任何旧文件移动） ============
  # v14 per codex v13 P2 #3：OUT_DIR 既有 tarball 与 .sha256 必须"两者都在
  # 或都不在"。半对状态下回滚无法保证恢复为完整旧对，故 fail-closed：
  # 不写 journal、不动任何文件、不清 .txn 之外的东西（.txn 由本分支 trap/
  # 显式清理）。
  OLD_TAR_EXISTS=0; OLD_SHA_EXISTS=0
  [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst" ] && OLD_TAR_EXISTS=1
  [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" ] && OLD_SHA_EXISTS=1
  if [ "$OLD_TAR_EXISTS" != "$OLD_SHA_EXISTS" ]; then
    echo "ERROR: OUT_DIR 既有快照不完整（仅存 tarball 或仅存 .sha256，半对）→ fail-closed exit 9" >&2
    echo "  协议不变量：最终状态只能是完整旧对或完整新对；请 dsh 人工补齐或删除半对后重跑" >&2
    rm -rf -- "$TXN_DIR"
    exit 9
  fi
  NEW_TAR_SHA=$(LC_ALL=C sha256sum "$STAGE_TAR" | LC_ALL=C awk '{print $1}')
  NEW_TAR_SIZE=$(stat -c %s "$STAGE_TAR")
  OLD_TAR_PLAN=absent; OLD_SHA_PLAN=absent
  OLD_TAR_SHA=none; OLD_SHA_SHA=none
  if [ "$OLD_TAR_EXISTS" = "1" ]; then
    OLD_TAR_PLAN=present
    # v16 per codex v15 P1 #1：旧对精确指纹（恢复时严格匹配，杜绝把
    # 未知/损坏/替换内容误判为旧文件）
    OLD_TAR_SHA=$(LC_ALL=C sha256sum "${OUT_DIR}/d-stage-snapshot.tar.zst" | LC_ALL=C awk '{print $1}')
  fi
  if [ "$OLD_SHA_EXISTS" = "1" ]; then
    OLD_SHA_PLAN=present
    # v17 per codex v16 P2 #3：staged 落盘前**旧对自洽校验**——sidecar 必须
    # 恰好一行 + 精确格式 + hash 字段 == 旧 tarball 的 SHA-256，且整个旧对
    # 必须 sha256sum -c 通过。内容彼此不匹配但格式合法的"旧对"不得进入
    # 事务（否则若新快照正常提交，损坏证据会被后续清理静默丢弃）。
    # v19 per codex v18 P2 #3：本校验块是 **sidecar 严格 schema 的唯一判定
    # 入口**（恰好一行 + 精确格式 + hash 字段），幂等 no-op 分支必须复用
    # 本块结论，不得用裸 sha256sum -c 代替（GNU sha256sum -c 对两行重复
    # 合法行仍返回 0，会放行 malformed/duplicate sidecar）。
    { [ "$(LC_ALL=C wc -l < "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" | LC_ALL=C tr -d ' ')" = "1" ] \
      && LC_ALL=C grep -qEx '[0-9a-f]{64}  d-stage-snapshot\.tar\.zst' \
           "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256"; } || {
      echo "ERROR: OUT_DIR 既有 .sha256 格式非法（非恰好一行 '<64hex>  d-stage-snapshot.tar.zst'）→ fail-closed exit 9" >&2
      rm -rf -- "$TXN_DIR"
      exit 9
    }
    OLD_SHA_HASH_FIELD=$(LC_ALL=C awk '{print $1}' "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256")
    [ "$OLD_SHA_HASH_FIELD" = "$OLD_TAR_SHA" ] || {
      echo "ERROR: OUT_DIR 既有 .sha256 hash 字段（${OLD_SHA_HASH_FIELD}）与旧 tarball SHA-256（${OLD_TAR_SHA}）不一致 → 旧对不自洽 fail-closed exit 9" >&2
      rm -rf -- "$TXN_DIR"
      exit 9
    }
    ( cd "${OUT_DIR}" && LC_ALL=C sha256sum -c \
        "d-stage-snapshot.tar.zst.sha256" ) >/dev/null || {
      echo "ERROR: OUT_DIR 既有旧对 sha256sum -c 自校验失败（内容不匹配）→ fail-closed exit 9" >&2
      rm -rf -- "$TXN_DIR"
      exit 9
    }
    OLD_SHA_SHA=$(LC_ALL=C sha256sum "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" | LC_ALL=C awk '{print $1}')
  fi
  # v18 per codex v17 P1 #1 + v19 per codex v18 P2 #3：幂等 no-op 前置——
  # 确定性重跑产生与既有快照完全相同输出时（旧、新指纹相等），尚未移动的
  # 旧对会被 classify_out_pair 判为 new（staged 状态禁止 new）→ 恢复必
  # exit 9。因此在写 journal 之前检测"既有对 == staging 新对"→ 直接清理
  # staging 并成功返回，不写 journal、不移动任何文件。
  # v19 修订：no-op 判定**复用上方已通过的严格 sidecar 校验结论**（恰好
  # 一行 + 精确格式 + hash 字段 == OLD_TAR_SHA + sha256sum -c 通过），
  # 仅比较 OLD_TAR_SHA == NEW_TAR_SHA；不再单独跑裸 sha256sum -c。
  # 两行重复合法行等 malformed sidecar 已在上方 exit 9，绝不进入 no-op。
  if [ "$OLD_TAR_EXISTS" = "1" ] && [ "$OLD_SHA_EXISTS" = "1" ] \
     && [ "$OLD_TAR_SHA" = "$NEW_TAR_SHA" ]; then
    echo "INFO: 既有快照与新快照内容相同且严格自洽（确定性重跑幂等 no-op）→ 清理 staging 并成功返回"
    rm -rf -- "$TXN_DIR"
    exit 0
  fi
  # v19 per codex v18 P1 #1：staging 数据持久化屏障——persist_journal(staged)
  # 之前必须先把新 tarball / .sha256 文件内容与其目录项持久化（fsync 文件
  # + fsync staging 目录），否则断电后可能出现"journal=staged 已持久、
  # staging 文件缺失或旧内容"→ 下次启动只能 exit 9。任一失败 → exit 5
  # （journal 尚未写入；清理 .txn；旧对未动，无数据丢失）。
  fsync_file "$STAGE_TAR" || { echo "ERROR: staging tarball fsync 失败 → exit 5" >&2; rm -rf -- "$TXN_DIR"; exit 5; }
  fsync_file "$STAGE_SHA"  || { echo "ERROR: staging sha256 fsync 失败 → exit 5" >&2; rm -rf -- "$TXN_DIR"; exit 5; }
  fsync_dir "$STAGE_DIR"   || { echo "ERROR: staging 目录 fsync 失败 → exit 5" >&2; rm -rf -- "$TXN_DIR"; exit 5; }
  trap - EXIT   # commit 阶段开始：此后一切由 journal 状态机接管
  if ! persist_journal "$JOURNAL" \
       "schema=1" "state=staged" "staging_dir=${STAGE_DIR}" \
       "new_tar_sha256=${NEW_TAR_SHA}" "new_tar_size=${NEW_TAR_SIZE}" \
       "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
       "old_tar_sha256=${OLD_TAR_SHA}" "old_sha_sha256=${OLD_SHA_SHA}"; then
    echo "ERROR: journal(staged) 写入失败（此时旧对尚未移动，无数据丢失）" >&2
    rm -rf -- "$TXN_DIR"
    exit 5
  fi
fi

# ============ 4b. 备份旧对（v13 per codex v12 P1 #3：staged 已落盘后才允许；守卫幂等） ============
if [ "$OLD_TAR_PLAN" = "present" ] && [ ! -e "$OLD_TAR" ]; then
  if ! mv -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst" "$OLD_TAR"; then
    echo "ERROR: 备份旧 tarball 失败（journal 保持 staged，旧对未丢；修复后重跑自动续跑）" >&2
    exit 5
  fi
fi
if [ "$OLD_SHA_PLAN" = "present" ] && [ ! -e "$OLD_SHA" ]; then
  if ! mv -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" "$OLD_SHA"; then
    echo "ERROR: 备份旧 sha256 失败（journal 保持 staged；下次启动从 staged 续跑）" >&2
    exit 5
  fi
fi
# v19 per codex v18 P1 #1 + v20 per codex v19 P1 #1：备份 mv 后 fsync .txn
# （目标）与 OUT_DIR（源）两个目录再 persist(backed_up)——跨目录 rename 只
# 同步目标目录会在断电后残留 OUT_DIR 源目录项（旧文件双存在 → 恢复 exit 9，
# 与 backed_up power-loss 自动续跑矛盾）；只同步源目录则 .txn 目录项可能
# 丢失（old.* 缺失 → exit 9）。
fsync_dir "$TXN_DIR" || {
  echo "ERROR: .txn 目录 fsync 失败（journal 保持 staged；facts 可恢复）→ exit 5" >&2
  exit 5
}
fsync_dir "$OUT_DIR" || {
  echo "ERROR: OUT_DIR 源目录 fsync 失败（journal 保持 staged；facts 可恢复）→ exit 5" >&2
  exit 5
}
if ! persist_journal "$JOURNAL" \
     "schema=1" "state=backed_up" "staging_dir=${STAGE_DIR}" \
     "new_tar_sha256=${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" \
     "new_tar_size=${NEW_TAR_SIZE_J:-$NEW_TAR_SIZE}" \
     "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
     "old_tar_sha256=${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" \
     "old_sha_sha256=${OLD_SHA_SHA_J:-$OLD_SHA_SHA}"; then
  echo "ERROR: journal(backed_up) 写入失败（journal 仍为 staged；下次启动从 staged 依据事实续跑，无数据丢失）" >&2
  exit 5
fi

# ============ 4c. 提交新 tarball（守卫幂等） ============
if [ ! -e "${OUT_DIR}/d-stage-snapshot.tar.zst" ]; then
  if ! mv -f -- "$STAGE_TAR" "${OUT_DIR}/d-stage-snapshot.tar.zst"; then
    echo "ERROR: 提交新 tarball 失败（journal 保持 backed_up，可恢复）" >&2
    exit 5
  fi
fi
# v19 per codex v18 P1 #1：新 tar mv 到 OUT_DIR 后先 fsync OUT_DIR 目录再
# persist(tarball_committed)，否则断电后 journal=tarball_committed 但 OUT_DIR
# 目录项可能丢失 → 事实一致性 exit 9。
fsync_dir "$OUT_DIR" || {
  echo "ERROR: OUT_DIR 目录 fsync 失败（journal 保持 backed_up；facts 可恢复）→ exit 5" >&2
  exit 5
}
if ! persist_journal "$JOURNAL" \
     "schema=1" "state=tarball_committed" "staging_dir=${STAGE_DIR}" \
     "new_tar_sha256=${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" \
     "new_tar_size=${NEW_TAR_SIZE_J:-$NEW_TAR_SIZE}" \
     "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
     "old_tar_sha256=${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" \
     "old_sha_sha256=${OLD_SHA_SHA_J:-$OLD_SHA_SHA}"; then
  echo "ERROR: journal(tarball_committed) 写入失败（journal 仍为 backed_up；facts 已含新 tarball，恢复自动跳过本步）" >&2
  exit 5
fi

# ============ 4d. 提交新 sha256（守卫幂等） ============
if [ ! -e "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" ]; then
  if ! mv -f -- "$STAGE_SHA" "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256"; then
    echo "ERROR: 提交新 sha256 失败（journal 保持 tarball_committed，可恢复）" >&2
    exit 5
  fi
fi
# v19 per codex v18 P1 #1：新 sha mv 到 OUT_DIR 后先 fsync OUT_DIR 目录再
# persist(sha_committed)。
fsync_dir "$OUT_DIR" || {
  echo "ERROR: OUT_DIR 目录 fsync 失败（journal 保持 tarball_committed；facts 可恢复）→ exit 5" >&2
  exit 5
}
if ! persist_journal "$JOURNAL" \
     "schema=1" "state=sha_committed" "staging_dir=${STAGE_DIR}" \
     "new_tar_sha256=${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" \
     "new_tar_size=${NEW_TAR_SIZE_J:-$NEW_TAR_SIZE}" \
     "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
     "old_tar_sha256=${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" \
     "old_sha_sha256=${OLD_SHA_SHA_J:-$OLD_SHA_SHA}"; then
  echo "ERROR: journal(sha_committed) 写入失败（journal 仍为 tarball_committed；facts 已含新对，恢复自动跳过本步）" >&2
  exit 5
fi

# ============ 4e. 最终自校验 → verified 或 rolling_back ============
if ( cd "${OUT_DIR}" && LC_ALL=C sha256sum -c \
       "d-stage-snapshot.tar.zst.sha256" ) >/dev/null; then
  if ! persist_journal "$JOURNAL" \
       "schema=1" "state=verified" "staging_dir=${STAGE_DIR}" \
       "new_tar_sha256=${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" \
       "new_tar_size=${NEW_TAR_SIZE_J:-$NEW_TAR_SIZE}" \
       "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
       "old_tar_sha256=${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" \
       "old_sha_sha256=${OLD_SHA_SHA_J:-$OLD_SHA_SHA}"; then
    echo "ERROR: journal(verified) 写入失败（journal 仍为 sha_committed；下次启动重做自校验）" >&2
    exit 5
  fi
  # 清理（守卫幂等）：rm old → fsync .txn → rm journal → rm -rf txn → fsync OUT_DIR
  rm -f -- "$OLD_TAR" "$OLD_SHA"
  # v21 per codex v20 P1 #3：rm old.* 后必须先 fsync_dir(.txn) 再删 journal——
  # 否则断电后可能出现"journal 删除已持久但 old.* 删除未持久"→ 下次启动
  # 走无 journal 分支因 old.* 存在 exit 9（与 verified 清理幂等声明矛盾）。
  fsync_dir "$TXN_DIR" || {
    echo "ERROR: .txn 目录 fsync 失败（journal 仍为 verified；下次启动重做清理，幂等）→ exit 5" >&2
    exit 5
  }
  rm -f -- "$JOURNAL"
  rm -rf -- "$TXN_DIR"
  # v19 per codex v18 P1 #1：清理完成后 fsync OUT_DIR 目录（journal/.txn
  # 目录项删除持久化）；否则断电后可能重现旧 journal（state=verified 重入
  # 清理，幂等安全）或 .txn 残留（v21 per codex v20 P1 #2：残留 .txn 无
  # journal 且无 old.* → 全新路径 → 4a 幂等 no-op 收敛）。
  fsync_dir "$OUT_DIR" || {
    echo "ERROR: OUT_DIR 目录 fsync 失败（清理已执行；重跑幂等）→ exit 5" >&2
    exit 5
  }
  echo "OK: ${OUT_DIR}/d-stage-snapshot.tar.zst (size: $(stat -c %s "${OUT_DIR}/d-stage-snapshot.tar.zst") bytes, reconcile 4 字段匹配, 自校验 PASS, journal 状态机完成)"
  echo "下一步：与阶段二 release commit 一同提交（详 §五.3 v24）"
  exit 0
else
  echo "ERROR: 最终路径 sha256sum -c 自校验失败 → 进入回滚" >&2
  if ! persist_journal "$JOURNAL" \
       "schema=1" "state=rolling_back" "staging_dir=${STAGE_DIR}" \
       "new_tar_sha256=${NEW_TAR_SHA_J:-$NEW_TAR_SHA}" \
       "new_tar_size=${NEW_TAR_SIZE_J:-$NEW_TAR_SIZE}" \
       "old_tar=${OLD_TAR_PLAN}" "old_sha=${OLD_SHA_PLAN}" \
       "old_tar_sha256=${OLD_TAR_SHA_J:-$OLD_TAR_SHA}" \
       "old_sha_sha256=${OLD_SHA_SHA_J:-$OLD_SHA_SHA}"; then
    echo "ERROR: journal(rolling_back) 写入失败（journal 仍为 sha_committed；下次启动重做自校验后再回滚，无数据丢失）" >&2
    exit 5
  fi
  # 回滚段（v17 per codex v14 P1 #2 + codex v15 P1 #1 + codex v16 P1 #1：
  # 严格指纹 + 共享分类函数 + 去 || true）。
  # v17 per codex v16 P1 #1：即时回滚与启动恢复**共用 classify_out_pair()**；
  # sidecar 只有在精确识别为 new（单行 + 精确格式 + hash 字段 == 新指纹）
  # 时才能删除；unknown / old → 保留 journal 与文件现场，exit 9。
  classify_out_pair
  if [ "$OUT_TAR_STATE" = "unknown" ] || [ "$OUT_TAR_STATE" = "old" ]; then
    echo "ERROR: OUT_DIR tarball 状态为 ${OUT_TAR_STATE}（非精确 new，拒绝删除；保留 journal 与现场）→ exit 9" >&2
    exit 9
  fi
  if [ "$OUT_SHA_STATE" = "unknown" ] || [ "$OUT_SHA_STATE" = "old" ]; then
    echo "ERROR: OUT_DIR .sha256 状态为 ${OUT_SHA_STATE}（非精确 new，拒绝删除；保留 journal 与现场）→ exit 9" >&2
    exit 9
  fi
  [ "$OUT_TAR_STATE" = "new" ] && rm -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst"
  [ "$OUT_SHA_STATE" = "new" ] && rm -f -- "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256"
  # v23 per codex v22 P1 #1：**每个**跨目录还原 mv 后立即 fsync 源、目标
  # 两个目录，再处理下一个文件（与恢复 rolling_back 分支同一协议）——
  # 两次还原 mv 之间断电时，若只在全部 mv 完成后统一 fsync，第一次 mv
  # 的源/目标目录项可能未持久 → 旧 tar 双存在或均缺失 → 三者事实校验
  # exit 9。逐次屏障后，(old,absent) 由持久化保证。
  # v24 per codex v23 P1 #1：本段 fsync_dir 失败统一 exit 5（与 §五 Python
  # 契约一致；journal 保持 rolling_back，下次启动重入收敛）；v24 per codex
  # v23 P2 #2：还原 mv 后、任一目录 fsync 未完成时断电 → 按被还原文件分列
  # 四态矩阵：旧 tar → target-only 白名单 (old,absent) / source-only
  # (absent,absent) 且 .txn 有 old.*；旧 sidecar（旧 tar 已持久还原）→
  # target-only 白名单 (old,old) / source-only (old,absent) 且 .txn 有 old.*；
  # 一致事实均白名单显式允许重入收敛；旧文件双存在 / 均缺失 → 三者事实
  # 校验 exit 9 保留证据（不得仅凭 OUT 状态对恰为合法中途态而错误接受
  # sidecar 丢失；见故障注入表 v24 新场景）。
  if [ -e "$OLD_TAR" ]; then
    mv -f -- "$OLD_TAR" "${OUT_DIR}/d-stage-snapshot.tar.zst" || {
      echo "ERROR: 回滚还原旧 tarball 失败 → exit 9（journal 保持 rolling_back，下次启动重入）" >&2
      exit 9
    }
    fsync_dir "$OUT_DIR" || {
      echo "ERROR: OUT_DIR 目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
      exit 5
    }
    fsync_dir "$TXN_DIR" || {
      echo "ERROR: .txn 源目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
      exit 5
    }
  fi
  if [ -e "$OLD_SHA" ]; then
    mv -f -- "$OLD_SHA" "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" || {
      echo "ERROR: 回滚还原旧 sha256 失败 → exit 9（journal 保持 rolling_back，下次启动重入）" >&2
      exit 9
    }
    fsync_dir "$OUT_DIR" || {
      echo "ERROR: OUT_DIR 目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
      exit 5
    }
    fsync_dir "$TXN_DIR" || {
      echo "ERROR: .txn 源目录 fsync 失败（v24 per codex v23 P1 #1：fsync 失败统一 exit 5，与 §五 Python 契约一致；journal 保持 rolling_back，下次启动重入收敛）→ exit 5" >&2
      exit 5
    }
  fi
  # 最终校验：旧对若存在必须自洽（sha256sum -c 通过），否则 exit 9
  if [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst" ] || [ -e "${OUT_DIR}/d-stage-snapshot.tar.zst.sha256" ]; then
    ( cd "${OUT_DIR}" && LC_ALL=C sha256sum -c \
        "d-stage-snapshot.tar.zst.sha256" ) >/dev/null || {
      echo "ERROR: 回滚后 OUT_DIR 旧对自校验失败（内容不匹配）→ exit 9" >&2
      exit 9
    }
  fi
  rm -f -- "$JOURNAL"
  rm -rf -- "$TXN_DIR"
  fsync_dir "$OUT_DIR" || {
    echo "ERROR: OUT_DIR 目录 fsync 失败（旧对已还原；重跑幂等）→ exit 5" >&2
    exit 5
  }
  echo "OK: 已回滚至完整旧对（OUT_DIR = 旧对）"
  exit 1
fi

# bash 草图等价 helper（真实实现 = tools/d-stage-audit-gen.py 内 python3
# 函数；v19 per codex v18 P1 #2：本 block 为伪代码，这些函数定义排在
# 调用点之后仅作语义参考，不代表可直接执行；persist_journal 语义见上方
# 注释 ①②③，fsync_file / fsync_dir 语义见 v19 状态依赖数据屏障注释；
# **严禁**只用全局 sync）：
fsync_file() {
  python3 - "$1" <<'PYEOF' || return 1
import os, sys
fd = os.open(sys.argv[1], os.O_RDONLY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
PYEOF
}
fsync_dir() {
  python3 - "$1" <<'PYEOF' || return 1
import os, sys
dfd = os.open(sys.argv[1], os.O_RDONLY | os.O_DIRECTORY)
try:
    os.fsync(dfd)
finally:
    os.close(dfd)
PYEOF
}
persist_journal() {
  local jpath="$1"; shift
  local tmp="${jpath}.tmp"
  { for kv in "$@"; do printf '%s\n' "$kv"; done; } > "$tmp" || return 1
  python3 - "$tmp" <<'PYEOF' || return 1
import os, sys
fd = os.open(sys.argv[1], os.O_RDONLY)
try:
    os.fsync(fd)
finally:
    os.close(fd)
PYEOF
  mv -f -- "$tmp" "$jpath" || return 1
  python3 - "$(dirname "$jpath")" <<'PYEOF' || return 1
import os, sys
dfd = os.open(sys.argv[1], os.O_RDONLY | os.O_DIRECTORY)
try:
    os.fsync(dfd)
finally:
    os.close(dfd)
PYEOF
}
```

**v24 P1 #1+P2 #2 故障注入测试要求**（per codex v13 P1 #1 + P1 #2 + P2 #3 + codex v14 P1 #1 + P1 #2 + codex v15 P1 #1 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2；共 29 场景：v14=11 / v15=12 / v16=13 / v17=16 / v18=16 / v19=22 / v20=25 / v21=28 / v22=28 / v23=28 / v24=29（v24 新增 1 个场景：回滚还原 mv 与逐次双目录 fsync 之间断电窗口 per codex v23 P2 #2，并统一回滚段逐次 fsync 失败退出码为 exit 5 per codex v23 P1 #1；v23 无新增场景，仅修订回滚还原逐次 mv 屏障与 power_loss_rollback_mid_restore_window 期望确定性 per codex v22 P1 #1；v22 无新增场景，仅修订 rolling_back 白名单可执行等价判定 per codex v21 P1 #1；v21 新增 3 个场景：rolling_back 中途删除窗口 / rolling_back 中途还原窗口 / verified 清理 journal 删除后 .txn 残留窗口；v20 新增 3 个跨目录 rename 断电窗口场景；v19 新增 5 个 power-loss 场景 + 1 个 no-op 重复行 sidecar 场景；v18 无新增场景））：

排他锁 + 持久化事务目录 + 结构化 journal 协议必须在 O-2 工具实现时附**故障注入测试**，至少覆盖以下场景
（v24 占位，待 O-1 全闭合 + tree-manifest 锁定后由 qoder 实现并由 codex 复审）：

| 场景 | 注入方式 | 期望最终状态（v24 重写） |
| --- | --- | --- |
| 并发第二个 snapshot（锁争用） | 进程 A 持锁运行中，启动进程 B | B 的 `acquire_snapshot_lock` 失败 → exit 6 + 明确错误信息；**B 在任何临时文件 / .txn / staging 变更之前即退出**（v15 per codex v14 P1 #1：锁先于一切文件系统变更）；A 正常完成 |
| staged 落盘后崩溃（备份 mv 之前） | `kill -9 $$` 在 journal(staged) 持久化之后、备份旧 tarball mv 之前 | 下次启动（重新获锁后）解析 journal=staged → 校验 staging_dir 路径约束 + staging 指纹（sha256/size 一致）→ 补做备份 mv → backed_up → 提交新对 → verified → 清理；最终 OUT_DIR = 新对 |
| 备份两个 mv 之间崩溃 | `kill -9 $$` 在旧 tarball 已 mv 入 .txn、旧 sha256 mv 之前 | journal=staged + facts（old tar 在 .txn、old sha 在 OUT_DIR）→ 恢复补做剩余备份 mv（守卫幂等）→ 继续；最终 OUT_DIR = 新对 |
| backed_up 落盘后崩溃（提交新 tarball 之前） | `kill -9 $$` 在 journal(backed_up) 之后、新 tarball mv 之前 | journal=backed_up → 恢复提交新 tarball + 新 sha256 → 自校验 → 清理；最终 OUT_DIR = 新对 |
| 新 tarball mv 与 journal(tarball_committed) 之间崩溃 | `kill -9 $$` 在新 tarball 已到 OUT_DIR、journal 写之前 | journal=backed_up + facts（新 tar 在 OUT_DIR，内容 = new_tar_sha256）→ 恢复跳过 tarball 提交、直接提交 sha256；最终 OUT_DIR = 新对 |
| 最终自校验失败（内容被篡改） | 故意写入错误 sha256 内容（不影响 file size） | 4e sha256sum -c FAIL → persist rolling_back → 回滚段调用 classify_out_pair：被篡改文件（tarball sha 或 sidecar hash 字段不再匹配新指纹）→ unknown → **exit 9 保留 journal 与现场**（fail-closed；v16 及以前"自动回滚到旧对"预期与 fail-closed 策略矛盾，已废弃 per codex v17 P1 #2；自动回滚路径仅存在于 rolling_back 崩溃重入且文件均为精确 new/old 的场景） |
| OUT_DIR 只读（备份 mv 失败） | `chmod -w OUT_DIR` 后跑 snapshot | 备份 mv 失败 → exit 5；journal 保持 staged；OUT_DIR = 旧对（未动）；修复权限后重跑从 staged 续跑 |
| journal 写入失败（filesystem 只读，发生在 staged 落盘时） | staged 落盘前使文件系统只读 | persist_journal(staged) 失败 → exit 5；**旧对从未移动（顺序保证）**；.txn 清理；无数据丢失 |
| journal 损坏（人为写 garbage） | `echo garbage > .txn/journal` 后跑 snapshot | 启动解析失败（schema≠1 / state 缺失）→ exit 9 + 明确错误信息；dsh 人工清理 |
| staging_dir 路径穿越 / symlink | 手工把 journal 的 staging_dir 改为 `.txn/../../etc` 或把 .txn/staging 替换为 symlink | 路径约束校验失败（非绝对 / canonical 越界 / symlink / 非目录）→ exit 9；不做任何 mv |
| OUT_DIR 既有半对（仅 tarball 或仅 .sha256） | 预置 OUT_DIR 仅 `d-stage-snapshot.tar.zst`（无 .sha256）后跑 snapshot | 4a 完整性 fail-closed → exit 9 + 明确错误信息；不写 journal、不动文件、OUT_DIR 保持半对（dsh 人工补齐或删除后重跑） |
| 旧对备份丢失（恢复期） | 事务进行中删除 `.txn/old.tar.zst`（journal old_tar=present）后重跑 | 事实一致性校验失败（计划 present 但 .txn 与 OUT_DIR 均不可证明旧 tarball）→ exit 9 + 明确错误信息；**不删新对、绝不静默恢复成半对**（dsh 人工介入） |
| OUT_DIR 未知内容（损坏/替换） | 事务进行中把 OUT_DIR tarball 替换为第三方文件（sha 既非 new_tar_sha256 也非 old_tar_sha256）后重跑 | 指纹分类 unknown → exit 9 + 明确错误信息；**不得把未知内容误判为旧文件**、不删除、不报告"完整旧对"（per codex v15 P1 #1） |
| 即时回滚 sidecar 未知（格式合法但 hash 字段错误） | 同进程回滚前把 OUT_DIR .sha256 改为格式合法但 hash 字段 ≠ new_tar_sha256 的内容 | 共享 classify_out_pair 分类为 unknown → exit 9；**sidecar 仅在精确识别为 new（单行 + 精确格式 + hash 字段）时才删除**，保留 journal 与现场（per codex v16 P1 #1） |
| verified 状态半对（仅 new tarball，缺 .sha256） | 手工构造 journal=verified + OUT_DIR 仅有 new tarball 后重跑 | 状态专属不变量失败（verified 必须两文件均 new）→ exit 9；**不清理 journal / 旧备份 / .txn 任何证据**（per codex v16 P1 #2） |
| 初始旧对不自洽（格式合法但 hash 字段 ≠ 旧 tarball SHA） | 预置 OUT_DIR 旧对：tarball 与 .sha256 内容互不匹配（但 .sha256 格式合法）后跑 snapshot | 4a 自洽校验失败（hash 字段 ≠ old_tar_sha256 或 sha256sum -c 失败）→ exit 9；**不自洽旧对不得进入事务，损坏证据不被静默丢弃**（per codex v16 P2 #3） |
| power-loss：staged 持久化后断电（备份 mv 之前） | persist_journal(staged) 完成后立即强制断电（VM poweroff / 拔盘；**不得用 kill -9 代替**——kill -9 不丢 page cache，覆盖不了断电语义） | journal=staged 与 staging 文件均持久（fsync 屏障保证，per codex v18 P1 #1）→ 重启恢复：路径约束 + staging 指纹校验通过 → 补做备份 mv → backed_up → 提交新对 → verified → 清理；最终 OUT_DIR = 新对（per codex v18 P1 #1） |
| power-loss：staged 持久化前断电（staging 已生成） | staging 文件写入后、persist_journal(staged) 前强制断电 | journal 缺失（.txn 存在但无 journal、无 old.* → 顺序不变式成立）→ 重启后丢弃 .txn 残留**继续全新路径**（v21 per codex v20 P1 #2：不再因"OUT_DIR 自洽"直接判为已完成新对——旧对完整 + staging 残留时旧路径会误判；若既有对与新快照内容相同则 4a 幂等 no-op 严格校验后 exit 0，否则正常进入事务）；旧对未动；重跑成功（per codex v18 P1 #1 + codex v20 P1 #2） |
| power-loss：backed_up 持久化后断电（提交新 tarball 之前） | persist_journal(backed_up) 完成后强制断电 | journal=backed_up 与 .txn/old.* 均持久（fsync 屏障 + 跨目录 rename 源、目标双目录同步，per codex v18 P1 #1 + codex v19 P1 #1）→ 恢复提交新 tarball + 新 sha256 → 自校验 → 清理；最终 OUT_DIR = 新对（per codex v18 P1 #1） |
| power-loss：新 tarball mv 后、journal(tarball_committed) 前断电 | 新 tar 已 mv 到 OUT_DIR 且 fsync_dir(OUT_DIR) 完成后、persist 前断电 | journal=backed_up + facts（新 tar 在 OUT_DIR 且内容 = new_tar_sha256，目录项已持久）→ 恢复跳过 tarball 提交、直接提交 sha256；最终 OUT_DIR = 新对（per codex v18 P1 #1） |
| power-loss：verified 持久化后、清理完成前断电 | persist_journal(verified) 完成后、rm journal / rm -rf .txn 与 fsync_dir(OUT_DIR) 完成前断电 | journal=verified（目录 fsync 未完成时可能重现）+ OUT_DIR 完整新对 → 状态专属不变量通过 → 重做清理 → exit 0；最终 OUT_DIR = 新对（清理幂等；per codex v18 P1 #1） |
| no-op 前 sidecar 重复行（两行均格式合法） | 预置 OUT_DIR 旧对：tarball 内容 == 新快照内容，但 .sha256 为两行重复的合法行（裸 sha256sum -c 会返回 0） | no-op 分支复用严格 sidecar 校验（恰好一行 + 精确格式 + hash 字段）→ 校验失败 exit 9；**malformed/duplicate sidecar 不被 no-op 接受**，损坏证据不被静默丢弃（per codex v18 P2 #3） |
| power-loss：备份 mv 后、persist(backed_up) 前断电 | 两个备份 mv 与 fsync_dir(.txn) + fsync_dir(OUT_DIR) 均完成后、persist_journal(backed_up) 前强制断电 | journal=staged（持久）+ facts（old 对仅在 .txn，OUT_DIR 源目录项删除已持久 → **无双存在**）→ 恢复：备份 mv 守卫幂等跳过 → persist(backed_up) → 提交新对 → verified → 清理；OUT_DIR = 新对（自动续跑，v20 per codex v19 P1 #1） |
| power-loss：备份 mv 与双目录 fsync 之间断电 | 备份 mv 完成后、fsync_dir(.txn) + fsync_dir(OUT_DIR) 任一未完成时强制断电 | journal=staged；facts 三态：(1) 目录项恰好一致 → 按 staged 事实自动续跑；(2) OUT_DIR 源目录项残留 → 旧文件双存在 → exit 9；(3) .txn 目录项未持久 → 均缺失 → exit 9。任何情况**绝不静默产生半对**（fsync 未完成残余窗口 fail-closed，v20 per codex v19 P1 #1） |
| power-loss：回滚还原 mv 后、清理完成前断电 | 回滚还原 mv + fsync_dir(OUT_DIR) + fsync_dir(.txn) 完成后、回滚收敛清理前强制断电 | journal=rolling_back + OUT_DIR 完整旧对（目标目录项持久）+ .txn 无 old.*（源目录删除持久 → **无双存在**）→ 重入回滚段：无 new 文件可删、无 old.* 可还原 → 自校验 PASS → 回滚收敛清理 = **目录级 rename tombstone**（os.rename(.txn → .txn.tombstone) + fsync_dir(OUT_DIR)，原子提交事务终态）→ exit 1；OUT_DIR = 完整旧对（v20 per codex v19 P1 #1 + v24 per codex 六轮复审 P1（阶段一工具实现轮，不升版）：rename 后断电 → .txn.tombstone 残骸由下次启动严格丢弃；rename 前断电 → journal 仍在，重入收敛） |
| power-loss：回滚中途删除窗口（已删新 tar、未删新 sidecar） | 回滚段 rm 新 tar 完成后、rm 新 sidecar 前强制断电 | journal=rolling_back + OUT_DIR 状态对 = (absent,new) → **rolling_back 合法中途态白名单**（v21 per codex v20 P1 #1）显式允许 → 重入回滚段：继续删新 sidecar → 逐个还原旧对 → 自校验 → 清理 → exit 1；OUT_DIR = 完整旧对（绝不 exit 9） |
| power-loss：回滚中途还原窗口（已还原旧 tar、未还原旧 sidecar） | 回滚段 mv 旧 tar 回 OUT_DIR 后（该 mv 的 fsync_dir(OUT_DIR) + fsync_dir(.txn) 已完成）、mv 旧 sidecar 前强制断电 | journal=rolling_back + OUT_DIR 状态对 = (old,absent)（**确定性**：v23 per codex v22 P1 #1 逐次还原 mv 屏障保证第一次 mv 的源、目标目录项已持久 → 无双存在/均缺失）→ **rolling_back 合法中途态白名单**显式允许 → 重入回滚段：无 new 文件可删、补做旧 sidecar 还原（mv 后同样逐次 fsync）→ 自校验 → 清理 → exit 1；OUT_DIR = 完整旧对 |
| power-loss：回滚还原 mv 与逐次双目录 fsync 之间断电 | 回滚段任一还原 mv（旧 tar 或旧 sidecar）完成后、该 mv 的 fsync_dir(OUT_DIR) + fsync_dir(.txn) 任一未完成时强制断电 | journal=rolling_back；facts 按**被还原文件**分列四态矩阵（v24 per codex v23 P2 #2，与备份方向 mv-fsync 窗口同一契约）：**旧 tar 的还原 mv** → (1) target-only（仅 OUT_DIR 有旧 tar）→ 白名单 (old,absent)；(2) source-only（仅 .txn 有 old.tar）→ (absent,absent) 且 .txn 有 old.*；**旧 sidecar 的还原 mv**（此时旧 tar 已持久还原 → OUT tar = old）→ (3) target-only（仅 OUT_DIR 有旧 sidecar）→ 白名单 (old,old)；(4) source-only（仅 .txn 有 old sidecar）→ (old,absent) 且 .txn 有 old.*——(1)-(4) 一致事实均被**白名单显式允许** → 重入回滚段幂等收敛；旧文件双存在（OUT_DIR + .txn 都有）→ 三者事实校验 exit 9（保留 journal 与现场）；目录项均缺失（OUT_DIR 与 .txn 都无）→ 三者事实校验 exit 9（保留证据；**绝不静默产生半对/空对，尤其不得仅凭 OUT 状态对恰为合法中途态而错误接受 sidecar 丢失**，fail-closed） |
| power-loss：verified 清理 journal 删除后、.txn 删除前断电 | 4e/verified 清理 rm old.* + fsync_dir(.txn) + rm journal 完成后、rm -rf .txn 前强制断电 | journal 缺失 + .txn 存在但无 old.*（v21 per codex v20 P1 #3：old.* 删除先于 journal 删除且已 fsync）→ 丢弃 .txn 残留继续全新路径（v21 per codex v20 P1 #2）→ 既有新对与新快照内容相同 → 4a 幂等 no-op 严格校验后 exit 0；OUT_DIR = 新对 |

故障注入测试**必须**在 O-2 工具实现时一并实现，测试结果存
`docs/planning/evidence/d-stage-audit.genesis.json` 的
`fault_injection_tests` 字段（v24 schema 共 29 场景；v14=11 / v15=12 /
v16=13 / v17=16 / v18=16 / v19=22 / v20=25 / v21=28 / v22=28 / v23=28 /
v24=29），
包含每场景的注入方式 + 实际最终状态 + 期望最终状态对照。
**power-loss 场景注入方式 = 强制断电（VM poweroff / 拔盘后重启），
不得用 kill -9 代替**（kill -9 不丢 page cache，无法覆盖断电语义）；
测试环境要求 OUT_DIR 所在文件系统为本地崩溃一致性文件系统（ext4/xfs
默认 barrier 语义），NFS/9P/virtiofs 已由 exit 78 排除。
**未经故障注入测试通过的 snapshot 子命令禁止在阶段二 release commit 中使用**。

**v24 P1 #1+P2 #2 已知 tradeoff（v23 6 项修订为 v24 6 项；旧对丢失项已消除）**：

1. **journal 写失败不再导致旧对丢失**（per codex v12 P1 #3）：journal
   持久化一律发生在对应动作**之前**（staged 含备份计划与指纹后才移动旧
   文件）；写失败窗口内旧对未动或已有更早的持久状态 + facts 可恢复。
   因此 v12 tradeoff #1（"journal 是 best-effort，写失败 → 旧对丢失"）
   **已删除**，不再作为接受条件。
2. **恢复依赖文件系统事实 + journal 下界**：journal 状态与磁盘事实可能
   存在一步错位（如 tarball 已 mv 但 journal 仍 backed_up）；恢复算法
   对每步 mv 都有存在性守卫，幂等重入；错位窗口内的任何崩溃都不会产生
   错位最终态。
3. **PID 复用风险已消除**：v13 使用固定事务目录 `.txn`（无 PID 命名），
   journal 记录 staging_dir 绝对路径 + 新文件指纹；恢复时重新绑定该路径
   并校验 sha256/size（不一致 → exit 9）；v14 进一步施加 staging_dir
   路径约束（realpath -m + is_within(.txn/staging) + 非 symlink + 目录）。
4. **NFS / 9P / virtiofs 等不保证 atomic rename 的文件系统禁止作为
   OUT_DIR**；O-2 工具启动时 `stat -f -c %T ${OUT_DIR}` 检测文件系统类型，
   NFS / 9p / virtiofs → exit 78。journal 目录 fsync（os.fsync dirfd）与
   flock 在这些文件系统上同样不可靠，属同一禁止理由。
5. **锁文件 `.snapshot.lock` 永不删除**（v14 per codex v13 P1 #1）：删除
   锁文件会破坏不同进程间的 flock 互斥（unlink 后新进程创建新 inode 重新
   加锁，两个进程可能同时持锁）；清理阶段只 rm .txn 内容，不 rm 锁文件。
6. **断电一致性依赖 fsync 屏障与本地崩溃一致性文件系统**（v19 per codex
   v18 P1 #1 + v20 per codex v19 P1 #1 + v24 per codex v23 P1 #1+P2 #2）：
   每个 journal 状态落盘前必须完成
   其依赖文件/目录的 fsync（staging 文件+目录 / .txn 目录 / OUT_DIR 目录，
   顺序不可交换）；**跨目录 rename（备份 OUT_DIR→.txn、回滚 .txn→OUT_DIR）
   必须同步源、目标两个目录**，否则断电后源目录残留目录项 → 旧文件双
   存在 → 恢复 exit 9。屏障失效（如 NFS 等不保证 barrier 语义的文件系统）
   → 断电后可能 exit 9（fail-closed，绝不静默产生半对）；NFS/9P/virtiofs
   已由 exit 78 排除。v24 细化：**运行时 fsync 失败一律 exit 5**（含回滚
   段逐次还原 mv 后的目录 fsync，与 §五 Python 契约统一，journal 保持
   rolling_back 重入收敛）；**断电落在"还原 mv 后、任一目录 fsync 未完成"
   窗口 → 三态契约**（一致事实可重入 / 旧文件双存在 / 均缺失 → 三者
   事实校验 exit 9 保留证据，fail-closed），与备份方向 mv-fsync 窗口
   同一契约（v24 per codex v23 P2 #2 故障注入场景覆盖）。

**reconcile 子命令规范（v24 per codex v15 P1 #2：唯一合法入口 =
`tools/d-stage-audit-gen.py reconcile`；禁止任何独立 shell 脚本入口，
与 030-d-stage-audit §五"唯一可执行入口 + 严禁独立入口"约束一致）**：

```bash
# 活动规范（v24）：reconcile 必须以子命令形式调用，命名参数与退出码
# 见 030-d-stage-audit.md §五 子命令 3：
tools/d-stage-audit-gen.py reconcile \
  --tarball docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst \
  --manifest docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv
# 退出码：0 = 4 字段全部匹配 / 1 = NF≠4 或字段编码错误 /
#         2 = reconcile 失败（4 字段比对不一致）/ 3 = tarball 或 manifest 不存在
```

以下 bash 片段**仅为历史对照**（v9-v11 演化记录，**不再是活动入口**；
v16 per codex v15 P1 #2：旧 $1/$2 位置参数入口与三子命令唯一入口、
命名参数约束、"禁止独立 shell 入口"直接矛盾，已废弃。若未来需要独立
命令行工具，必须包装为 `tools/d-stage-audit-gen.py reconcile` 的实现
内部逻辑，不得作为独立脚本对外暴露）：

```bash
#!/usr/bin/env bash
# 【历史对照 · 非活动规范】v9 tarball ↔ manifest reconcile（per codex v8
# P1 #2 + P1 #3；v16 起废弃独立入口，见上文活动规范）
# - NUL 安全：每类条目写入独立 .tsv 临时文件，NUL 结尾；最后 tr 转 LF；
#   不依赖 awk -v RS='\0'（gawk 扩展）以保证 POSIX awk / mawk 可重放。
# - 4 字段内容校验：regular file SHA-256 + symlink target 完整比对，
#   仅 (file_type, relative_path) 集合相等视为不充分（v8 缺陷）。
set -euo pipefail
LC_ALL=C
TARBALL="${1:?missing tarball}"
MANIFEST="${2:?missing D_stage manifest}"

RECONCILE_DIR="$(mktemp -d -t r16-reconcile-XXXXXX)"
RECONCILE_D="${RECONCILE_DIR}/.d.tsv"
RECONCILE_L="${RECONCILE_DIR}/.l.tsv"
RECONCILE_F="${RECONCILE_DIR}/.f.tsv"
DIFF_OUT="${RECONCILE_DIR}/diff.out"
trap 'rm -rf -- "$RECONCILE_DIR"' EXIT

# 1. 解压 tarball
tar --use-compress-program=zstd -xf "$TARBALL" -C "$RECONCILE_DIR"

# 2a. 目录条目（4 字段：d\t<rel>\t\t，NUL 结尾）
( cd "$RECONCILE_DIR" && \
  find d-stage -mindepth 0 -type d -printf 'd\t%P\t\t\0' ) \
  | LC_ALL=C sort -z > "$RECONCILE_D"

# 2b. symlink 条目（4 字段：l\t<rel>\t<target>\t，NUL 结尾）
( cd "$RECONCILE_DIR" && \
  find d-stage -mindepth 0 -type l -printf '%P\0' ) \
  | LC_ALL=C sort -z \
  | while IFS= read -r -d '' fpath; do
      target=$(LC_ALL=C readlink "${RECONCILE_DIR}/d-stage/${fpath}")
      printf 'l\t%s\t%s\t\0' "$fpath" "$target"
    done > "$RECONCILE_L"

# 2c. regular file 条目（4 字段：f\t<rel>\t\t<sha256>，NUL 结尾）
( cd "$RECONCILE_DIR" && \
  find d-stage -mindepth 0 -type f -printf '%P\0' ) \
  | LC_ALL=C sort -z \
  | while IFS= read -r -d '' fpath; do
      sha=$(LC_ALL=C sha256sum "${RECONCILE_DIR}/d-stage/${fpath}" \
              | LC_ALL=C awk '{print $1}')
      printf 'f\t%s\t\t%s\0' "$fpath" "$sha"
    done > "$RECONCILE_F"

# 3. 合并三类 + tr NUL→LF + **全局按 (file_type, relative_path) 排序**
#    + 校验 NF=4 + 字段编码
# v10 per codex v9 P1 #1：D/L/F 三类拼接顺序与 manifest 规范（d < f < l
# ASCII 序）冲突；必须显式按 (file_type, relative_path) 全局重排，否则
# 即使内容完全正确也会因行序不一致而 diff 失败
NEW_MANIFEST="${RECONCILE_DIR}/new.manifest.tsv"
cat -- "$RECONCILE_D" "$RECONCILE_L" "$RECONCILE_F" \
  | tr '\0' '\n' \
  | LC_ALL=C sort -t$'\t' -k1,1 -k2,2 \
  | LC_ALL=C awk -F'\t' '
      NF != 4 {
        print "ERROR: NF=" NF " at line " NR ": " $0 > "/dev/stderr"
        exit 1
      }
      {
        for (i = 1; i <= 4; i++) {
          if ($i ~ /[\t\r\n\0]/) {
            print "ERROR: control char in field " i " at line " NR > "/dev/stderr"
            exit 1
          }
        }
        print
      }
    ' > "$NEW_MANIFEST"

# 4. 完整 4 字段 diff（regular file SHA / symlink target 不一致立即 FAIL）
if LC_ALL=C diff -u "$MANIFEST" "$NEW_MANIFEST" > "$DIFF_OUT"; then
  rows=$(LC_ALL=C wc -l < "$NEW_MANIFEST")
  echo "OK: tarball ↔ manifest reconcile 通过（4 字段完整匹配，$rows 行）"
else
  cat "$DIFF_OUT" >&2
  echo "ERROR: tarball ↔ manifest reconcile 失败（4 字段比对不一致）" >&2
  exit 1
fi
```

**v9 per codex v8 P1 #2 + P1 #3 关键变化（vs v8）**：

| 维度 | v8 缺陷 | v9 修订 |
| --- | --- | --- |
| NUL record separator | awk 默认 RS=LF；3 find 流视为 1 行 | 每类条目独立 .tsv + sort -z + tr '\0' '\n'（POSIX awk 兼容，不依赖 gawk `-v RS='\0'`） |
| 字段数 | awk `print $1, $2`（仅 2 字段） | 4 字段完整保留：`type / path / symlink_target / file_sha256` |
| 内容校验 | 仅 (type, path) 集合相等 | 完整 4 字段 `diff -u`：regular file 内容 / symlink target 替换立即 FAIL |
| symlink target 重建 | 未做 | `readlink` 重新读取，与 manifest 字段 3 字节比对 |
| regular file SHA 重建 | 未做 | `sha256sum` 重新计算，与 manifest 字段 4 字节比对 |

**v10 P1 #7 + codex v8 P1 #1 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 关键约束清单**：

| 选项 | 值 | 目的 |
| --- | --- | --- |
| `tar --sort=name` | 强制 | 文件按名排序，不同文件系统目录项顺序差异消除 |
| `tar --mtime=$EPOCH` | `@1640995200` | 固定 mtime（避免时间戳漂移） |
| `tar --owner=0 --group=0` | uid/gid = 0 | 固定属主 |
| `tar --numeric-owner` | 强制 | 不解析用户名（避免主机差异） |
| `tar --no-acls --no-xattrs --no-selinux` | 强制 | 不保留扩展属性（避免文件系统差异；v9 per codex v8 P1 #1：`--no-seclinux` 不存在于 GNU tar，已删除；三选项完整覆盖 POSIX ACL + xattr + SELinux context） |
| `tar --transform='s,^\.,d-stage,'` | 强制 | tar 内路径前缀改为 `d-stage`（避免绝对路径污染） |
| `zstd -q -19` | level 19 | 固定压缩等级（高 + 可复现） |
| `tar` + `zstd` 版本 | **精确锁定**（v10 per codex v9 P2 #6，本机实测）：`tar 1.35` + `zstd 1.5.7` | 跨机器 SHA 一致必须**精确**锁定这两个版本；任何其他版本一律 exit 78，工具代码内显式拒绝（不采用最低版本约束，因为不同 GNU tar / zstd 版本的 header / 压缩字节流可能不同）；genesis.json `tool_versions.tar/zstd` + `snapshot_tool_versions` + `snapshot_boundary` 同步记录；详 §五.3 v21 + O-2 §十一 v21 genesis.json schema |

**严禁**：

- 使用 `tar -czf`（gzip）：未固定 mtime + sort，不可复现；
- 使用 `tar` 默认 sort：不固定顺序，不同机器目录项顺序可能不同；
- 使用 `tar --preserve-permissions` / `--same-owner`：依赖运行环境用户；
- 写入 `migration/supervised-source-tree/` 任何子路径（违反保护区）；

严禁：
- 任何 patches/030-NNN.*（阶段三才允许）
- 任何 patches/4.0.2-i3.*（v5 模糊例外已取消；元数据走 meta.json）
- 任何 drivers/ / baselines/ / binary-manifest.json 修改
- 任何 migration/supervised-source-tree/ 子路径（监督分支绝对保护区；
  v6 `_r16-d-stage/` 在 v8 中仍维持取消状态）
- 任何阶段三产物（O_stage / 5.0.0-i1）
- 任何 F0 引用 / 内容 / 命名空间
- 任何外部不可变制品（所有 O-2 输入 lock + D_stage 快照必须**作为提交内容**
  嵌入 commit；不得依赖外部 tarball / artifact store）
```

**v24 复核要求**：阶段二 release commit 完成时，`git diff --cached --name-status`
输出必须**精确**列出上述 4 类内容中所有文件路径（路径完整、状态 = A/M/D
明示）；任何遗漏（含 O-2 输入 manifest + .sha256 + D_stage 快照 tarball +
.sha256）视为 release commit 不完整，**禁止**签发阶段二 tag
（**v24 per codex v7 P2 #8**：tag 名称必须为 dsh 唯一确认的唯一名称，
从 `4.0.2-i3.meta.json` `git_tag_ref` 字段读取；详 §5.4 v24 + §九.2 v24）。

### 5.4 阶段二 tag（不可移动的 annotated tag，**v24 per codex v7 P2 #8 + codex v9 P2 #7 + codex v11 P2 #4**）

**v7 P2 #7 + v7 P2 #8 核心约束**：Git tag 身份必须在阶段二启动**前**由 dsh 单独确认，
qoder / codex **不得**单方面决定或候选化（v7 "候选命名（待 dsh 确认）"
已被 codex v7 P2 #8 标记为不可接受——必须 1 个唯一名称 + 显式 dsh 批准）；
**v10 per codex v9 P2 #7**：tag 名称**禁止硬编码**为包版本号 `4.0.2-i3`，
必须从 `4.0.2-i3.meta.json` 的 `git_tag_ref` 字段读取（详 §九.2 v24）。

- **tag 名称确认流程（v10 强制）**：
  1. dsh 在阶段二启动前执行 `git tag -l` 反查仓库既有 tag 命名约定；
  2. dsh 给**唯一**一个 tag 名称（不允许候选列表）；
  3. dsh 在 `4.0.2-i3.meta.json` 起草前显式书面确认该名称（写入 `git_tag_ref` 字段）；
  4. qoder 仅按 dsh 确认的唯一名称生成 tag，禁止自创 / 改写 / 候选化；
- **关键区分（v10 per codex v7 P2 #8 + codex v9 P2 #7）**：
  - **包版本号** = `4.0.2-i3`（dpkg 元数据 `Version:` 字段，固定不变）；
  - **Git ref** = dsh 唯一确认的 tag 名称（**不一定**等于 `4.0.2-i3`，
    **可能**含前缀如 `r16-4.0.2-i3` / `v4.0.2-i3` / 仓库既有约定）；
  - 文档 / shell 脚本 / `4.0.2-i3.meta.json` 中凡引用 Git tag 处必须使用
    dsh 确认的唯一名称，**禁止**将包版本号当作 Git tag 名直接调用 `git
    tag` / `git checkout` / `git rev-parse` 等 ref 操作；
- **tag 类型**：annotated tag（不可移动）；
- **tag 签发条件**（任一未满足不发 tag）：
  - dsh 已书面确认唯一 tag 名称（v10 per codex v7 P2 #8 + codex v9 P2 #7 新增硬条件；写入 `4.0.2-i3.meta.json` `git_tag_ref` 字段）；
  - 独立 release commit 已完成（不与阶段三 / O-1 / O-2 文档 commit 混合）；
  - 双 clean-build 复现通过（同输入同 epoch 字节一致）；
  - 包载荷 / 许可 / 安装回退验证全 PASS；
  - 阶段二验证矩阵全 PASS；
  - 三方一致 + dsh 终审 + 用户批准（仅允许批准后签发 tag）；
- **tag 后的边界**：
  - dsh 确认的 Git tag = Deepin 血缘终止点 + 阶段三 F0 迁移起点；
  - 未来 Deepin 延续**必须**从该 tag 另开分支，**不得**混入 030 血缘；
  - F0 / R-F 迁移**必须**从该 tag 分出新分支，不得在 Deepin 血统分支上
    直接叠加 030-NNN。

### 5.5 阶段二允许 / 禁止

| 维度 | 允许 | 禁止 |
| --- | --- | --- |
| **输入** | 阶段一产物（D_stage + 中性记录 + D→D_stage 审计） | F0 任何引用；阶段三产物 |
| **决策** | 包版本号 / tag 命名 / dpkg 元数据 / 构建参数 | absorb / adapt / rewrite / 任何 F0 比较决策 |
| **产物** | release commit + annotated tag + 包 + 元数据 + 双构建证据 + 阶段二测试结果 | 030-NNN / F0 任何内容 |
| **写入路径** | `4.0.2-i3.meta.json`（release commit 内容）+ `docs/planning/evidence/4.0.2-i3/`（验证证据 + D_stage 快照 tarball；**v13 per codex v7 P1 #1 + P1 #5 + P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v10 P1 #1+#2 + codex v11 P1 #1+#2 + codex v12 P1 #1-#4**：D_stage 快照走 `docs/planning/evidence/4.0.2-i3/d-stage-snapshot.tar.zst`，目录级 staging + reconcile-first + 持久化事务目录 + 结构化 journal + 精确版本锁，tar.zst 可复现规范详 §5.3 v24，不再写入 `_r16-d-stage/`） | 任何保护区路径（详 §十一）；**`patches/` 顶层（含 patches/4.0.2-i3.* / patches/030-NNN.*）任何写入**（v5 "如仓库约定允许写入 patches/4.0.2-i3.*" 模糊例外已取消）；**`migration/supervised-source-tree/` 任何子路径**（v7 监督分支绝对保护区，不再以"dsh 显式批准"放行） |
| **tag 操作** | 阶段二 release commit 的 annotated tag 签发 | 在 Deepin 血统既有 tag / commit 上修改 / amend / force-push |

### 5.6 阶段二门禁

- **O-3（阶段二子项）**：阶段二运行时 fixture 验证 `4.0.2-i3` 基线（详 §七.1）；
- **门禁通过判定**：
  - 双 clean-build 复现通过；
  - 包载荷 / 许可 / 安装回退全 PASS；
  - 阶段二验证矩阵全 PASS（不依赖 F0 任何内容）；
  - 三方一致 + dsh 终审 + 用户批准（仅允许批准后签发 `4.0.2-i3` tag）。

## 六、阶段三：fantgpu 基座迁移（`4.0.2-i3` + F0 → O_stage → `5.0.0-i1`）

### 6.1 阶段三输入

| 输入 | 说明 | 来源 |
| --- | --- | --- |
| **`4.0.2-i3` tag** | 阶段二输出（SHA-256 锁定） | 阶段二 release commit |
| **`4.0.2-i3` Deepin 中性变更记录** | 阶段一 / 阶段二产物 | `030-mapping-table.md` + `4.0.2-i3.meta.json` |
| **F0 源树** | fantgpu 3.3.8.126 源树快照（SHA-256 锁定） | dsh 提供（**阶段三前置 = O-4**） |
| **F0 文件 / tree hash** | 每个 F0 文件的 SHA-256 + tree hash | 由 O-4 闭合后从 F0 源树计算 |

### 6.2 阶段三输出

- **`O_stage` 源树**：F0 + 已批准的 030-NNN × N 应用后的 R-F staging；
- **每条 030-NNN 独立记录**：
  - 来源 `4.0.2-i3` 中性变更记录（来源 patch / 目标路径 / 语义 / 依赖 / 顺序）；
  - F0 对应位置（F0 文件路径 + SHA-256 + tree hash）；
  - F0 比较结果（四分类之一）：**已覆盖** / **实现更好** / **实现不同** / **缺失**；
  - 类目裁决（仅当 4 项审查全通过后填入）：`absorb` / `adapt` / `rewrite` /
    `base-retain` / `no-030`；
  - 依赖（前序 030-NNN / 后继 030-NNN / 共享结构 / 共享回调）；
  - 验证结果（静态 + 阶段三运行时判据）；
  - patch hash（patch 内容的 SHA-256）；
  - 回退点（rollback 到 `4.0.2-i3` 或 O_stage 前一状态的精确命令）；
- **`5.0.0-i1` release**：阶段三最终 release commit + annotated tag + 包；
- **完整验收矩阵**：阶段三验证矩阵全 PASS（详 §七.2）。

### 6.3 阶段三判定准则（per-record）

> 对 `4.0.2-i3` 的**每条**中性变更记录，阶段三逐项执行：
>
> 1. **F0 比较**：F0 端是否已覆盖 / 实现更好 / 实现不同 / 缺失？
> 2. **语义裁决**：阶段三语义裁决小组给明确 verdict；
> 3. **许可证审查**：来源 D 端 SPDX + 来源 F0 端 SPDX + 上游 notice 要求 +
>    移植后 notice 判定；**NOASSERTION / 机密 / 未知来源直接拒绝**；
> 4. **依赖审查**：与 O_stage 已批准的 030-NNN 是否有依赖冲突；
> 5. **O_stage 适配**：决定 `absorb` / `adapt` / `rewrite` / `base-retain` /
>    `no-030`；
> 6. **三方一致 + 用户批准**：仅当 1-5 全通过 + 三方一致 + 用户批准后才
>    写入 030-NNN 并应用。
>
> 阶段三**逐项应用和验证**，最后才跑完整验收矩阵。

### 6.4 阶段三允许 / 禁止

| 维度 | 允许 | 禁止 |
| --- | --- | --- |
| **输入** | `4.0.2-i3` tag + 中性记录 + F0 源树 + F0 hash | 阶段二产物；阶段一产物 |
| **决策** | 4 项审查 + 类目裁决 + 030-NNN 应用 + `5.0.0-i1` tag | 在阶段一 / 阶段二产物上修改 / 覆盖 |
| **产物** | 030-NNN × N + O_stage + 5.0.0-i1 + 阶段三测试结果 | 在 `4.0.2-i3` 既有 commit / tag 上修改 / amend |
| **写入路径** | `patches/030-NNN.{patch,meta.json}`（仅阶段三）+ `docs/planning/evidence/o-stage/` | 任何保护区路径（详 §十一）；Deepin 血统既有 tag |

### 6.5 阶段三门禁

- **O-3（阶段三子项）**：阶段三 F0 迁移测试 fixture 验证每个 030-NNN +
  最终 O_stage（详 §七.2）；
- **O-4**：F0 源树就位 + SHA-256 锁定；
- **门禁通过判定**：
  - 每条 030-NNN 通过 4 项审查（语义 + 许可 + 依赖 + O_stage 适配）；
  - 每条 030-NNN 通过阶段三验证（静态 + 运行时）；
  - `O_stage` 完整验收矩阵全 PASS；
  - 三方一致 + dsh 终审 + 用户批准（仅允许批准后签发 `5.0.0-i1` tag）。

## 七、验证入口（两套证据分开记录，**不可互相替代**）

### 7.1 阶段二验证矩阵（验证 `4.0.2-i3` 基线，不涉及 F0）

| 维度 | 验证项 | fixture / 工具 |
| --- | --- | --- |
| **源码复现** | 双 clean-build 字节一致 | `scripts/build.sh` + 确定性构建门禁 |
| **包载荷** | dpkg-deb 校验 + 控制字段 | `dpkg-deb -I` / `dpkg-deb -c` |
| **许可** | `tools/audit-licenses.py` 无新 NOASSERTION / 机密路径 | `tools/audit-licenses.py` |
| **DKMS / 模块加载** | 模块 vermagic / 符号验证 | `tests/unit/run-r16-build-bc-map-tests.sh` |
| **运行时** | DRM device open / fbdev mmap / DMA-BUF self-import / VA-API 解码 / suspend/resume（Deepin-derived staging 既有能力） | `tools/run-dmabuf-regression-test.sh`（self-import 子集）+ `tools/run-vaapi-decode-test.sh` + `tools/probe-suspend-resume-state.sh` + `tests/runtime/run-capability-baseline.sh` |
| **安装 / 回退** | dpkg 安装成功 + 完整系统快照回退成功 | 手动记录 + 验证脚本 |
| **门禁汇总** | `scripts/check-docs.sh` rc=0 + `tools/validate-collab.py` rc=0 + `tools/r16-gate.py` rc=0 | 仓库既有门禁 |

**阶段二验证的硬约束**：

- **不得引用任何 F0 内容**（路径 / SHA / 命名空间 / 文件）；
- 不得引用阶段三的任何 fixture / 工具；
- 验证结果存储：`docs/planning/evidence/4.0.2-i3/`（非保护区）；
- 任何阶段二验证失败 → 阶段二 tag 不得签发 → 阶段三不得启动。

### 7.2 阶段三验证矩阵（验证每个 030-NNN + 最终 O_stage）

| 维度 | 验证项 | fixture / 工具 |
| --- | --- | --- |
| **静态** | 编译通过 + schema 校验 + 文档校验 + 许可校验 | `gcc` / `clang` + `tests/unit/run-030-*.sh` + `scripts/check-docs.sh` + `tools/audit-licenses.py` |
| **per-030 运行时** | 每个 030-NNN 的运行时判据（按语义 BC 分组，详 §七.2 子表） | 详下 |
| **O_stage 整体验收** | 全栈功能 + 性能基线 + 错误零增长 | 仓库既有 CI + 新增 fixture |
| **回退验证** | 每个 030-NNN 回退到 O_stage 前一状态成功 | `patches/030-NNN.meta.json` 的 rollback 字段 |

**per-030 运行时判据**（按 BC 分组，UNVERIFIED 显式标注）：

| 语义分组 | 运行时判据 | 已存在的 fixture / 工具 | UNVERIFIED 标注 |
| --- | --- | --- | --- |
| BC-04 srvkm/ftx-public-headers | header 静态可用性 + 编译通过 | `tests/unit/run-r16-build-bc-map-tests.sh` | — |
| BC-07 srvkm/pdp-headers | DMA-BUF self-import + pdp 探测 | `tools/run-dmabuf-regression-test.sh` + `tools/probe-pdp-invisible-read.c` | cross-device PRIME 维持 UNVERIFIED |
| BC-09 srvkm/dpu-display | DRM device open + drm-topology + vblank probe | `tools/probe-drm-topology.c` + `tools/probe-drm-vblank.c` | fbdev mmap 集成 UNVERIFIED |
| BC-13 gpu/hal | 编译通过 + 单元断言 | `tests/unit/run-r16-*.sh` 编译套件 | hal suspend/resume 真机 UNVERIFIED |
| BC-16 dma | DMA-BUF self-import | `tools/run-dmabuf-regression-test.sh` + `tools/probe-dmabuf-self-import.c` | vblank 守卫 + cross-device UNVERIFIED |
| BC-17 power | suspend/resume 真机观测 | `tools/probe-suspend-resume-state.sh` + `tests/unit/run-suspend-resume-tests.sh` | rail gating 维持 UNVERIFIED |
| BC-18 vpu | VA-API 解码 | `tools/run-vaapi-decode-test.sh` + `tests/unit/run-vaapi-decode-tests.sh` + `tools/probe-vaapi.c` | 长测 UNVERIFIED |
| 通用能力基线 | 设备基础能力探测 | `tests/runtime/run-capability-baseline.sh` | — |

**阶段三验证的硬约束**：

- **每条 030-NNN 必须独立验证**，不得以阶段二通过替代 030-NNN 验证；
- **O_stage 整体验收**不得以阶段二验证矩阵替代；
- 阶段三 fixture 与阶段二 fixture **必须分开记录**（不同路径 + 不同元数据
  文件 + 不同 SHA-256），**互相不可替代**；
- 阶段三 runtime fixture 缺位（vblank / fbdev mmap / rail gating / hal suspend /
  VA-API 长测 / cross-device PRIME）→ 阶段三不得进入用户批准阶段；
- O-3 必须确认每个缺位 fixture 是否新增，未登记前不得写入 030-NNN 字段。

### 7.3 两套验证矩阵的边界

| 维度 | 阶段二验证 | 阶段三验证 |
| --- | --- | --- |
| **目标** | `4.0.2-i3` 基线（Deepin 血缘） | O_stage = `5.0.0-i1`（F0 + 030-NNN） |
| **F0 引用** | **禁止** | 必须 |
| **030-NNN 引用** | **禁止** | 必须 |
| **fixture 集** | 阶段二 fixture 子集 | 阶段三 fixture 子集（含 BC 分组） |
| **结果路径** | `docs/planning/evidence/4.0.2-i3/` | `docs/planning/evidence/o-stage/` |
| **签发条件** | 双 clean-build + 阶段二矩阵全 PASS + 三方一致 + 用户批准 → `4.0.2-i3` tag | per-030 + 整体验收矩阵全 PASS + 三方一致 + 用户批准 → `5.0.0-i1` tag |
| **互替性** | **不可**替代阶段三 | **不可**替代阶段二 |

## 八、流程顺序（三阶段硬约束）

### 8.1 三阶段流程图

```
[阶段一：Deepin 修改归纳]
   qoder
   D + 000-029 patch + P5 台账 → D_stage + Deepin 中性变更记录
        │
        ▼ O-1（19 → 中性记录）+ O-2（D → D_stage 完整性）
        │
[阶段二：Deepin 基座冻结与 4.0.2-i3 发布]
   qoder → dsh
   D_stage → 4.0.2-i3 release commit + annotated tag + 包 + 双构建复现
        │
        ▼ 阶段二验证矩阵 + 双 clean-build + 三方一致 + dsh 终审 + 用户批准
        │
        ├───────────────┬───────────────────────────────┐
        ▼               ▼                               ▼
[Deepin continuation]  [阶段三：fantgpu 迁移]            （未来其它路线）
   （从 4.0.2-i3 另开    qoder → codex → dsh
    分支继续维护；        4.0.2-i3 中性记录 + F0 → O_stage
    不涉及 F0）          逐项 4 项审查 → 030-NNN × N
                              │
                              ▼ O-3（阶段三）+ O-4（F0 源树）+ 三方一致 + 用户批准
                              │
                       [5.0.0-i1 release commit + annotated tag]
```

### 8.2 步骤细则

1. **阶段一**（qoder）：D + 000-029 patch + P5 台账 → D_stage + Deepin 中性变更记录；
2. **阶段一 O-1 + O-2 闭合**：19 项 → 中性记录 + D → D_stage 完整性审计；
3. **阶段二**（qoder → dsh）：D_stage → 4.0.2-i3 release commit + annotated tag +
   包 + 双构建复现 + 阶段二验证矩阵；
4. **阶段二批准**：三方一致 + dsh 终审 + 用户批准 → 签发 `4.0.2-i3` tag；
5. **阶段三启动**：O-4 闭合（F0 源树就位 + SHA-256 锁定），从 `4.0.2-i3`
   分出独立分支；
6. **阶段三**（qoder → codex → dsh）：逐项 4 项审查 + 030-NNN × N + O_stage +
   阶段三验证矩阵；
7. **阶段三批准**：三方一致 + dsh 终审 + 用户批准 → 签发 `5.0.0-i1` tag；
8. **Deepin continuation**：从 `4.0.2-i3` 另开分支继续 Deepin 维护（不涉及 F0）。

**批准前不得 `git commit` 任何阶段三内容**；批准后 commit 与 P5 同等粒度。

### 8.3 终审前置条件（三阶段分别闭合，per §〇.1）

终审（dsh + 用户）**必须**满足（per §〇.1 分阶段门槛）：

- **阶段一终审前置**：O-1（19 → 中性记录）+ O-2（D → D_stage 完整性，含
  tree-manifest + symlink 闭合）全闭合；**不等待 O-3 / O-4**（v5 全局
  写法已替换为 §〇.1 分阶段门槛）；
- **阶段二终审前置**：阶段一通过 + 阶段二 O-3（运行时 fixture 真机/VM
  路径确认）+ 双 clean-build 复现 + 阶段二验证矩阵全 PASS；
- **阶段三终审前置**：阶段二通过 + 阶段三 O-3（F0 迁移 fixture）+ O-4
  （F0 源树就位 + SHA-256 锁定）+ 每条 030-NNN 4 项审查 + 阶段三验证
  矩阵全 PASS。

**每阶段独立闭合 → 独立终审 → 独立批准 → 进入下一阶段**（per §〇.1）；
**严禁** 把任一阶段的 O-X 写成另一阶段终审的阻塞条件（v5 §〇 / §八.3
全写法已修订）。

## 九、`4.0.2-i3` tag 的双面角色

### 9.1 Deepin 血缘终止 / 冻结点

- `4.0.2-i3` 是 Deepin 4.0.x 血统的**终止点**；
- 阶段二签发后，Deepin 血统的所有未来修改必须从 `4.0.2-i3` 另开分支继续，
  **不得**直接在 `4.0.2-i3` 上叠加；
- `4.0.2-i3` 的 patch 清单与元数据完整保留（`4.0.2-i3.meta.json` + 双构建
  复现证据），任何 Deepin 延续分支可基于该 tag + 中性记录重放。

### 9.2 F0 迁移起点

- `4.0.2-i3` 是阶段三 F0 迁移的**起点**：从中性变更记录出发，与 F0 逐项
  比较；
- 阶段三分支必须从 dsh 唯一确认的 Git tag ref 分出（**v11 per codex v9 P2 #7
  + codex v10 P1 #3 修订**：禁止直用包版本号 `4.0.2-i3` 作为 `git checkout`
  的 ref——包版本号是 dpkg `Version:` 字段，Git ref 是 dsh 在
  `docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json` 的 `git_tag_ref`
  字段中确认的唯一名称，可能含 `r16-` / `v` 等仓库约定前缀；
  **v11 per codex v10 P1 #3 路径统一**：meta.json 路径**必须**与 §5.2
  阶段二产物清单一致 = `docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json`；
  阶段三启动前由工具链自校验 jq 版本是否符合 tool_versions.jq 前置条件）：
  ```bash
  # v11 强制：从 docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json 读取
  # dsh 确认的 Git ref；jq 1.7.1+ 工具链前置条件（per codex v10 P1 #3
  # 写入 genesis.json tool_versions.jq）
  META_PATH="docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json"
  REQUIRED_JQ_VER="1.7.1"
  JQ_VER=$(jq --version | LC_ALL=C grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
  if [ "$JQ_VER" != "$REQUIRED_JQ_VER" ]; then
    echo "ERROR: jq version must be exactly $REQUIRED_JQ_VER (got $JQ_VER)" >&2
    exit 78
  fi
  GIT_TAG_REF=$(jq -r '.git_tag_ref' "$META_PATH")
  if [ -z "$GIT_TAG_REF" ] || [ "$GIT_TAG_REF" = "null" ]; then
    echo "ERROR: ${META_PATH} git_tag_ref 字段缺失或未确认" >&2
    echo "  per codex v7 P2 #8：Git ref 必须在阶段二启动前由 dsh 唯一确认" >&2
    exit 1
  fi
  git checkout -b r-f-migration "$GIT_TAG_REF"
  ```
- **不得**在 Deepin 血统既有分支上叠加 030-NNN；
- **严禁**硬编码 `4.0.2-i3` 作为 `git checkout -b` 的 ref（若 dsh 确认的
  ref 是 `r16-4.0.2-i3`，硬编码会导致"reference not found"失败）。

### 9.3 双面角色支持未来维护

- 从 `4.0.2-i3` 可继续两条独立路线：
  - **Deepin continuation**：仅 Deepin 4.0.x 血统的延续（不涉及 F0）；
  - **F0 / R-F migration**：基于中性记录 + F0 逐项裁决（独立分支，最终
    演化为 `5.0.0-i1`）；
- 两条路线互不污染；任一路线的问题不影响另一条线的稳定性。

## 十、未决项（v24 分阶段门槛）

| 编号 | 事项 | 阶段归属 | 责任方 | 截止 | 配套文件 / 输出路径 | 状态（v24） |
| --- | --- | --- | --- | --- | --- | --- |
| **O-1** | 19 项台账 → Deepin 中性变更记录（**隔离 F-only**，**双处置列**） | **阶段一** | qoder | 阶段一 codex 初审前 | `docs/planning/030-mapping-table.md`（v12 三列分离 + F-only excluded-deferred 隔离） | v12 三列分离 + F-only excluded-deferred 隔离；v12 仅配套标签同步（per codex v12 P2 #5） |
| **O-2** | D → D_stage 完整性审计（**tree-manifest 锁定** + **9 类 symlink 闭合 + `realpath -m`** + **SYM-N-A reconcile** + **目录级 staging** + **OUT_DIR 排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal（含旧对精确指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 旧对完整性 fail-closed + 幂等 no-op（复用严格 sidecar 校验）+ fsync 状态依赖数据屏障（跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行等价判定）+ 无 journal 恢复走全新路径 + selfcheck fail-closed + reconcile 子命令唯一入口** + **三子命令接口** + **bash 草案降级伪代码 + Python 实现契约** + **精确版本锁** + **退出码作用域契约**） | **阶段一** | qoder | 阶段一 codex 初审前 | `docs/planning/030-d-stage-audit.md`（v24 在 v23 基础上叠加 回滚段逐次 fsync 失败退出码统一 exit 5 + power_loss_rollback_restore_fsync_window 三态契约场景 + 29 场景故障注入（+1 场景）） | v24 已替换 sha256sum<dir> → tree-manifest + 9 类 symlink 闭合 + `realpath -m` canonicalize + 6 fixture + SYM-N-A 不强制 BC 追溯 reconcile + 目录级 staging + reconcile-first + 固定事务目录 `.txn` + 结构化 journal 协议（含 old_tar_sha256/old_sha_sha256 指纹）+ 16 场景故障注入（计数口径修正）+ `--reference-manifest` CLI 解析（三类命名参数统一）+ 三子命令接口 + tar.zst 精确版本锁 + 退出码子命令作用域契约 + 排他锁退出码 6 + journal 损坏退出码 9 + fsync 状态依赖数据屏障（断电一致性）+ bash 草案降级伪代码 + §五 Python 实现契约 + 22 场景故障注入（5 power-loss + 1 no-op 重复行）+ 跨目录 rename 源、目标双目录同步 + 3 个跨目录 rename 断电窗口场景 + 25 场景故障注入 + reconcile 活动规范标签统一 v20 + rolling_back 合法中途态白名单 + 无 journal 恢复走全新路径 + verified 清理 .txn 屏障 + 3 个新场景 + 28 场景故障注入 + 白名单可执行等价判定（per codex v21 P1 #1）+ 回滚还原逐次 mv 屏障（per codex v22 P1 #1）+ 回滚段逐次 fsync 失败 exit 5 统一 + 回滚还原 fsync 窗口场景（per codex v23 P1 #1 + P2 #2）（per codex v9 5 P1 + 3 P2 + codex v10 4 P1 + 1 P2 + codex v11 2 P1 + 2 P2 + codex v12 4 P1 + 1 P2 + codex v13 2 P1 + 2 P2 + codex v14 2 P1 + 2 P2 + codex v15 2 P1 + 1 P2 + codex v16 2 P1 + 1 P2 + codex v17 2 P1 + 2 P2 + codex v18 2 P1 + 1 P2 + codex v19 1 P1 + 1 P2 + codex v20 3 P1 + codex v21 1 P1 + codex v22 1 P1 闭环 + codex v23 1 P1 + 1 P2 闭环） |
| **O-3（阶段二子项）** | 阶段二运行时 fixture 真机/VM 路径确认 | **阶段二**（**不阻塞阶段一**） | dsh | 阶段二 codex 初审前 | 验证结果存 `docs/planning/evidence/4.0.2-i3/` | 待启动（**阶段一可独立进入 dsh 终审**） |
| **O-3（阶段三子项）** | 阶段三 F0 迁移 fixture 真机/VM 路径确认 | **阶段三**（**不阻塞阶段一 / 阶段二**） | dsh | 阶段三 codex 初审前 | 验证结果存 `docs/planning/evidence/o-stage/` | 待启动（**阶段一 / 阶段二可独立进入终审**） |
| **O-4** | F0 端（fantgpu 3.3.8.126）源树就位 + SHA-256 锁定 | **阶段三前置**（**阶段二完成 + 用户阶段二批准后才启动**） | dsh | 阶段三启动前 | — | 待启动（**绝不阻塞阶段一**；v5 §〇 全局阻塞已替换为 §〇.1 分阶段门槛） |
| **O-5** | codex 初审 finding 闭环 | 每阶段独立 | qoder | codex 复审通过 | — | 待启动 |
| **O-6** | dsh 终审 | 每阶段独立 | dsh | dsh 复审通过 | — | 待启动 |
| **O-7** | 用户最终批准（**分阶段**：阶段一 / 阶段二 / 阶段三 各一次批准） | 每阶段独立 | 用户 | 每阶段启动前 | — | 待启动 |

**严禁全局断言（v6 已删除）**：v5 §〇 / §八.3 中"任一 O-1..O-4 未完成 →
不具终审条件"的全局写法已删除；现在每个 O-X 在哪个阶段才成为硬阻塞，
由上表"阶段归属"列明确。**阶段一不得等 O-4**，**阶段二不得等 O-3（阶段三）**。

## 十一、不在 R-F 前置范围内（保护区清单，三阶段通用）

**v9 patches/ 规则 reconcile（per codex v6 P2 #6 + codex v7 闭合复核 + v12 保持 v11 政策）**：

- 顶层 `patches/` 由 v6 的"绝对保护区（含精确例外）"二分描述改为
  **三层规则**（消除"绝对保护区 + 例外可写"自相矛盾）：
  1. **`patches/000-029/`**（Deepin 既有 000-029 patch 系列子目录）：
     **只读**，阶段一 / 阶段二 / 阶段三**全部禁止写入**；阶段一脚本只读取。
  2. **`patches/` 顶层（`patches/<其他子项或文件>`）**：阶段一 / 阶段二
     **禁止写入**；阶段三允许写入 `patches/030-NNN.{patch,meta.json}`
     （O-4 闭合 + 用户阶段三批准后）— 此为**唯一允许写入路径**，**不属于**
     顶层绝对保护区，**属于** 阶段三产物路径（详 §六.4）；
  3. **`patches/4.0.2-i3.*`**：**永久禁止写入**（v5 模糊例外已删除）；阶段二
     release 元数据走 `4.0.2-i3.meta.json` + 验证证据走
     `docs/planning/evidence/4.0.2-i3/`。
- **严禁**"绝对保护区 + 精确例外可由 dsh 批准"措辞：v6 此措辞同时给出
  禁止和允许，逻辑矛盾；v7 改为"三层规则"显式定义每层政策，不再有例外
  审批需要。
- 阶段一 / 阶段二 commit 中**不出现**任何 `patches/` 路径下的内容
  （除 `patches/000-029/` 只读引用）；阶段三 commit 中允许出现
  `patches/030-NNN.{patch,meta.json}`，**严禁**出现
  `patches/4.0.2-i3.*`。

**v12 D_stage 物化根 reconcile（per codex v6 P1 #1 + codex v7 P1 #7 tar.zst 可复现规范 + v12 复核保持）**：

- `migration/supervised-source-tree/` 任何子路径（含 v6 中提到的
  `_r16-d-stage/`）**全部禁止写入**（监督分支绝对保护区 per
  `docs/project/multiagent-collab.md:40-41`，**任何 dsh 批准都不能
  改称非保护区**）；
- D_stage 物化根 = `/tmp/r16-d-stage/`（v24 唯一合法路径；详 §〇.5 / §四.1 / §五.3 v24）。

**绝对保护区（v24 三阶段通用，三阶段全部禁止写入）**：

- `drivers/`、`baselines/`、`binary-manifest.json`、
  `patches/000-029/`（只读，详本节三层规则 #1）、
  `debs/`、`vendor/`、`build/`（**保护区，O-3 之前禁止写入**）、
  `third_party/`、`migration/supervised-source-tree/`（顶层，**所有子路径
  均不得写入**，D 源树只读访问除外；详本节 D_stage 物化根 reconcile）；
- 任何外部推送（remotes 配置由 dsh 确认）；
- 任何试图预填阶段三产物（030-NNN 类目 / O_stage 状态）的草稿行为；
- 任何伪造 F0 → Git 血缘的描述（必须以 F0 源快照为参考生成 patch）；
- 任何阶段二产物混入阶段三内容（**两阶段 tag / commit / fixture 严格分离**）；
- 任何在 `4.0.2-i3` 既有 commit / tag 上修改 / amend / force-push；
- 任何阶段三产物写入 Deepin 血统既有分支。

**显式非保护区（v24 显式标注，per codex v7 P1 #6 稳定证据 vs 运行时元数据拆分 + v24 复核保持）**：

- `/tmp/r16-d-stage/`：**R-F 工作目录**（v24 唯一合法 D_stage 物化根；
  系统 `$TMPDIR`，**不在仓库任何路径下**；详 §〇.5 + §四.1）；
- `docs/planning/evidence/d-stage-audit.*`：O-2 输出 **8 稳定证据文件**
  （主表 + symlink 专表 + 各自 .sha256 + 2 输入 manifest + 各自 .sha256）
  + **1 运行时元数据 `genesis.json`**（不计 SHA 一致判定）；
  v24 共 9 文件（详 §〇.5 + §五.2 + O-2 §十一 v24）；
- `docs/planning/evidence/4.0.2-i3/`：阶段二验证证据 + D_stage 快照
  tarball（v24：`d-stage-snapshot.tar.zst` + `.sha256`，**v24 per codex v7
  P1 #7 + codex v8 P1 #1 + P1 #4 + P2 #6 + codex v9 P1 #2 + P1 #3 + P2 #6
  + codex v10 P1 #1+#2 + codex v11 P1 #1+#2 + codex v12 P1 #1-#4
  + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15
  P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3
  + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 +
  codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 +
  codex v23 P1 #1+P2 #2**
  tar.zst 可复现规范详 §五.3 v24；目录级 staging + reconcile-first +
  排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal（含旧对精确
  指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享
  分类函数）+ 状态专属不变量 + 旧对自洽校验 + 旧对完整性 fail-closed +
  幂等 no-op（复用严格 sidecar 校验）+ fsync 状态依赖数据屏障（跨目录
  rename 源、目标双目录同步；verified 清理 rm old.* 后 fsync .txn）+
  rolling_back 合法中途态白名单（集合成员判断可执行等价判定）+ 精确
  版本锁；详 §〇.5 + §五.2）；
- `docs/planning/evidence/o-stage/`：阶段三验证证据；
- `docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json`（v11 per codex v10
  P1 #3 路径统一：**不放仓库根**，与 §9.2 F0 迁移起点 read 路径一致）：
  阶段二 release 元数据；
- `patches/030-NNN.{patch,meta.json}`（**仅阶段三允许**，详本节三层规则 #2）。

**例外审批规则（v7 删除 v6 模糊例外措辞；v10 复核保持）**：v6 §十一 中"任何对绝对保护区
的精确例外（包括精确子路径 + 精确文件名）必须由 dsh 显式批准"的措辞
与 §〇.6 "patches/ 顶层 = 绝对保护区" 形成逻辑矛盾（既绝对保护又可批准
例外）。v7 改为本节三层规则，每层政策**预先显式定义**，**不依赖** dsh
后续例外审批；`migration/supervised-source-tree/` 任何子路径仍为绝对
保护区，**不接受**任何 dsh 例外（per
`docs/project/multiagent-collab.md:40-41`）；v10 复核**未发现**需
新增例外审批项。

## 十二、附录：本文件自身的元数据

| 项 | 值 |
| --- | --- |
| 文件路径 | `docs/planning/030-patch-rederivation-design.md` |
| 创建日期 | 2026-09-05 |
| 起草 | qoder |
| 版本 | **v24**（codex v9 初审 5 P1 + 3 P2 闭环后 + codex v10 初审 4 P1 + 1 P2 闭环后 + codex v11 初审 2 P1 + 2 P2 闭环后 + codex v12 初审 4 P1 + 1 P2 闭环后 + codex v13 初审 2 P1 + 2 P2 闭环后 + codex v14 初审 2 P1 + 2 P2 闭环后 + codex v15 初审 2 P1 + 1 P2 闭环后 + codex v16 初审 2 P1 + 1 P2 闭环后 + codex v17 初审 2 P1 + 2 P2 闭环后 + codex v18 初审 2 P1 + 1 P2 闭环后 + codex v19 初审 1 P1 + 1 P2 闭环后 + codex v20 初审 3 P1 闭环后 + codex v21 初审 1 P1 闭环后 + codex v22 初审 1 P1 闭环后 + codex v23 初审 1 P1 + 1 P2 闭环后；本版：reconcile 全局按 (file_type, relative_path) 排序 / 目录级 staging + reconcile-first + 固定事务目录 `.txn` + 结构化 journal 协议 + journal 先于移动（per codex v12 P1 #1-#3）/ OUT_DIR 排他锁 fcntl.flock（先于一切变更）+ staging_dir 路径约束 + 旧对完整性 fail-closed（per codex v13 P1 #1+#2 + P2 #3）+ 锁时序前移 + 三者事实一致性 fail-closed + 回滚内容感知（per codex v14 P1 #1+#2）/ 旧对精确指纹 + 未知内容 exit 9 + reconcile 子命令唯一入口（per codex v15 P1 #1+#2）/ 共享分类函数 + 状态专属不变量 + 旧对自洽校验（per codex v16 P1 #1+#2 + P2 #3）/ 4a 幂等 no-op（旧新指纹相等成功返回）+ selfcheck_fail 期望 fail-closed + 故障注入计数 16 场景修正（per codex v17 P1 #1+#2 + P2 #3）/ fsync 状态依赖数据屏障（staging/.txn/OUT_DIR 目录持久化顺序，断电一致性）+ 5 个 power-loss 故障注入场景 + bash 草案降级算法伪代码 + audit §五 Python 实现契约 + no-op 复用严格 sidecar 校验 + noop_duplicate_sidecar 故障场景（per codex v18 P1 #1+#2 + P2 #3）/ 跨目录 rename 源、目标双目录同步（备份 .txn + OUT_DIR；回滚 OUT_DIR + .txn）+ 3 个跨目录 rename 断电窗口故障场景 + 故障注入 25 场景 + reconcile 活动规范标签统一 v20（per codex v19 P1 #1 + P2 #2）/ rolling_back 合法中途态白名单（统一混合判定仅对非 rolling_back 生效）+ 2 个回滚中途窗口场景 + 无 journal 恢复走全新路径（废除"OUT_DIR 自洽即 exit 0"捷径）+ 1 个 journal 删除后 .txn 残留窗口场景 + verified 清理顺序 rm old.* → fsync_dir(.txn) → rm journal → rm -rf .txn → fsync_dir(OUT_DIR) + 故障注入 28 场景（per codex v20 P1 #1+#2+#3）/ rolling_back 白名单可执行等价判定（shell 带引号精确匹配 + Python 集合成员判断 rolling_back_mid_state_ok，per codex v21 P1 #1）/ 9 类 symlink 跨树判定 `realpath -m` canonicalize 强化 + 6 fixture 覆盖（per codex v9 P1 #4）/ `tools/d-stage-audit-gen.py` 三个子命令 gen-manifest + snapshot + reconcile 接口（per codex v9 P1 #5）/ tar.zst 精确版本锁定 tar 1.35 + zstd 1.5.7 工具代码内显式拒绝非目标版本（per codex v9 P2 #6）/ 阶段三 `git checkout -b` ref 改为 `$GIT_TAG_REF` 从 4.0.2-i3.meta.json `git_tag_ref` 字段读取，禁止硬编码（per codex v9 P2 #7）/ `--reference-manifest` 实际 CLI 解析 + 三类命名参数统一解析 + 未知参数 fail-closed（per codex v11 P1 #2 + codex v14 P2 #4）/ 退出码子命令作用域契约（per codex v11 P2 #4）/ 文件 fsync + rename + 目录 fsync 显式路径（per codex v12 P1 #4）/ 全文档当前状态标签统一 v12（per codex v12 P2 #5）/ 退出码契约当前标签统一 v14（per codex v13 P2 #4）/ 活动接口标签统一 v15（per codex v14 P2 #3）/ 活动规范标签统一 v16（per codex v15 P2 #3）/ 活动规范 v16/v15 标签残留统一 v18（per codex v17 P2 #4）/ 活动规范标签统一 v19（per codex v18 P1 #2 伪代码降级配套）/ 活动规范标签统一 v20（per codex v19 P2 #2）/ 活动规范标签统一 v21（per codex v20 P1 #2 无 journal 恢复配套）/ 活动规范标签统一 v22（per codex v21 P1 #1 白名单修复配套）/ 回滚段 fsync 失败退出码统一 exit 5 + 回滚还原 fsync 窗口场景（per codex v23 P1 #1+P2 #2）） |
| 状态 | 设计稿（进行中，未具终审条件） |
| 三阶段 | 阶段一：D → D_stage / 阶段二：D_stage → 4.0.2-i3 / 阶段三：4.0.2-i3 + F0 → O_stage → 5.0.0-i1 |
| 上下游 | 上游 = P5；下游 = R-F 第一批（仅阶段三） |
| 审批链 | codex → dsh → 用户（**分阶段三次审批**） |
| 配套记录 | `collab/R16-2026-09-03-基座更新迭代评估/{qoder-notes,report}.md` |
| 配套产物 | `docs/planning/030-mapping-table.md`（O-1 v12 P5/阶段一/阶段三 三列分离 + F-only excluded-deferred 隔离）、`docs/planning/030-d-stage-audit.md`（O-2 v24 tree-manifest `SRC_ROOT` 显式 export + symlink 9 类互斥 + F0 完全隔离 + 9 文件输出 = 8 稳定证据 + 1 运行时 genesis.json + tar.zst 可复现规范 + 目录级 staging + reconcile-first + 排他锁（先于一切变更）+ 固定事务目录 `.txn` + 结构化 journal 协议（含旧对精确指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 旧对完整性 fail-closed + 幂等 no-op（复用严格 sidecar 校验）+ selfcheck fail-closed + fsync 状态依赖数据屏障（断电一致性；跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（rolling_back_mid_state_ok 集合成员判断）+ 无 journal 恢复走全新路径 + §五 Python 实现契约（唯一实现依据）+ reconcile 子命令唯一入口 + `--reference-manifest` CLI 解析 + 退出码作用域契约 + `realpath -m` canonicalize）、`docs/planning/evidence/d-stage-audit.tsv` + `.sha256` + `.genesis.json` + `.symlink.tsv` + `.symlink.tsv.sha256` + `.D.manifest.tsv` + `.D.manifest.tsv.sha256` + `.D_stage.manifest.tsv` + `.D_stage.manifest.tsv.sha256`（O-2 输出 v24 共 9 文件 = 8 稳定证据 + 1 运行时 genesis.json，待阶段一启动后生成）、`docs/planning/evidence/4.0.2-i3/`（阶段二验证证据 + `d-stage-snapshot.tar.zst` + `.sha256`，**v24 per codex v9 P1 #2 + P1 #3 + P2 #6 + codex v10 P1 #1+#2 + codex v11 P1 #1+#2 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2** tar.zst 可复现规范详 §五.3 v24；D_stage 快照嵌入 release commit）、`docs/planning/evidence/o-stage/`（阶段三验证证据）、`docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json`（阶段二 release 元数据；**v11 per codex v10 P1 #3 路径统一到 evidence dir**；**v10 per codex v9 P2 #7** Git tag 名称从 `git_tag_ref` 字段读取，禁止硬编码）、`/tmp/r16-d-stage/`（**v24 唯一合法 D_stage 物化根**，系统 `$TMPDIR`，**不在仓库任何路径下**；详 §〇.5 + §四.1 + §五.3 v24） |
| 预计 review | codex 三阶段分别复审 + dsh 终审 + 用户分阶段批准 |

---

## 十三、双轨变更纪律（两条腿 · 2026-09-10 用户裁定）

与章程 §十一 同源；本节给出操作细则，自 O_stage 构建集成完成后对 5.0.0-iN
血缘生效。

- **patch 续编规则**：N 从 030 起续编（030-030、030-031…）；每条相对上一
  after_tree_hash（链基）；规格与既有 030-NNN 完全一致（diff -ruN、-p1
  --fuzz=0 可重放、零 .orig/.rej、4 目评审、draft→verified 生命周期、
  rollback 字段、台账引用）。
- **台账**：`030-mapping-table.md` 增加「5.x 变更台账」小节（或新建 5.x
  台账文档）；每条改动登记：来源、语义、验证状态、链基 / after hash。
- **印证门禁**：发布前用 `tools/o4-f0-lock-gen.py manifest`（或同族工具）
  重算 O_stage 物化树 hash，必须等于「F0 快照 + 全链 030-NNN 重放」树 hash；
  不一致即阻塞发布。
- **基座更换流程（未来）**：新 vendor 基座 = F0′ 锁定（O-4 流程复用）→
  全链 patch 逐项重放 + 适配 → O_stage′；语义裁决直接复用既有 meta 的
  review 字段，仅更新 o_stage_adaptation 与行号证据。禁止回到考古式逐文件
  归纳。
- **链起点**：R16 阶段三已落地的 13 条重放链（001→002→006→009→007→023→
  025→024→026→027→026-lifecycle→028→029，O_stage = 937e3710…）为链起点；
  关闭项（003/004/005/008/025-display/patch-000）终判在同链新基座下沿用。
