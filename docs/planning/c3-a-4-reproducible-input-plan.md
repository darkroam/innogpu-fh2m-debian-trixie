# C3-a ④ 可复现输入实施设计（builder F 载荷纳入）——起草稿 v12

- 日期：2026-09-12
- 状态：**设计稿**（先方案，不动 builder/manifest/保护区；实现待 codex 初审 + dsh 放行）。**前置已闭合**：③ 完整性（工具 `tools/r16-f-payload-integrity.py` + 三件证据 + 单测 54 项，dsh 终审通过，0eaff0e/b5d11ec 已提交）；① 来源、② 授权由 dsh 定档。v2 修正：codex 初审 3 P1（三集合建模、materialization 规则、md5sums 确定性）+ 1 建议（可复现输入口径收紧）。v3 修正：codex re-review 2 P1（restore 事务化 + DEBIAN/ 排除、M6 闭合 + postinst 行为固定）。v4 修正：codex re-review#2 2 P1（sw-fant-gl 生命周期矛盾裁决、restore journal 状态机）。v5 修正：codex re-review#3 1 P1（状态写入失败窗口——写前状态 + 存在性/内容复核）。v6 修正：codex re-review#4 2 P1（恢复分支幂等 + journal 字段路径安全约束）。v7 修正：codex re-review#5 4 P1 + 1 P2（逐状态存在性真值表、并发排他锁、mode 锁定、builder 改造清单补全、fsync 保证范围收窄）。v8 修正：codex re-review#6 1 P1 + 2 P2 + 1 契约（真值表矛盾修正、锁生命周期 O_NOFOLLOW+永不删除、PKG_DESC 版本串参数化、M2 来源判定改内容 hash+物化 trace）。v9 修正：codex re-review#7 1 P1 + 2 P2（真值表收紧为仅可产生组合、S_INPUT 隐含集合弃单值 set、materialize-trace 可执行契约、postinst 文案消歧）。v10 修正：codex re-review#8 2 P1（moving_new(pre=F) 正向组合修正为 (0,1,0)、trace 计数三分拆分 + 分源 SHA 校验）。v11 修正：codex re-review#9 1 P1（trace 四列 + kind 类型语义、目录不入 trace 由计数/mode/subtree hash 单独断言、sidecar 弃用 materialized_files 旧字段）。v12 修正：codex re-review#10 1 P1（O_stage regular 文件 mode 来源定稿 = 锁定快照承载，trace 对 ostage-kernel 行仅校验 SHA）。
- 依据：`5.0.0-i1-validation-plan.md` §三 C3-a 第 ④ 项 +「③ 对 ④ 的传导」（两项强制）+「C3-a 的 builder 改造点清单」（8 点）+ §二「C3 若选 a/b 的版本与 provenance 传导」+ dsh 2026-09-11 放行「④ qoder 可开工（技术项）」。
- **不变式（全设计约束，违反任一即 FAIL）**：
  1. materialize 五件产物与 `5.0.0-i1.meta.json` **不变**（O_stage 树 hash `937e3710…` 不变，本设计不动内核源树）；
  2. F 载荷**字节不入 git**（② 定档「仅自用、不分发」；`/vendor/`（.gitignore:26）、`/build/`（:27）、`/debs/*`（:9-10）既有本地保存策略维持）——**清单（含逐文件 SHA-256）入 git，载荷本地保存**，与 O 血统 `binary-manifest.json` + `vendor/` 同构；
  3. 双构建字节一致在 F 载荷下仍成立（构建输入逐文件 SHA 锁定 + 构建期确定性，含 md5sums 字节）；
  4. 保护区零写入：本设计**实现阶段**对 `vendor/` 的落库写入属保护区写入，需 dsh 单独授权（见 §七 待裁决项 1）；其余保护区（debs/build/third_party/drivers/baselines/patches）维持只读；
  5. C3-a 落地前不得放行真机批、不得交付 C1-②、不得为 F7 设默认身份串（validation-plan §三 禁止动作）。

## 〇、「可复现输入」口径（codex 建议收紧，显式声明）

本设计的可复现性口径 = **「本地重建可复现」（local-reproducible）**，不是「纯净工作区零外部依赖重建」：

- **给定** git 内容（manifest + builder + 工具 + 预选参数）+ 本地 `vendor/fantgpu/` 载荷，双 clean-build 字节一致**可独立复算**；
- **干净工作区重建输入的路径**是显式的：来源 deb `debs/fantgpu-fh2m_3.3.8.126-driver-linux-desktop-sp-generic_amd64.deb`（① 已定档「官方通道 + 元数据一致」的本地保存物）→ 恢复工具重新解包落库 → 逐文件 SHA 与 manifest 比对一致后方可构建（§一 恢复步骤）。deb 字节本身不在 git 内，其 SHA 由 manifest `source_deb_sha256` 锁定；
- **fail-closed 输入预检**（builder F 分支输入检查阶段，先于一切组装）：manifest 每条输入条目对应的 `vendor/fantgpu/` 文件/链接必须存在且 SHA 一致；任一缺失/漂移 → `builder_fpayload_input=FAIL` + 缺失清单，exit 1。**不得**以「本地可能已有等价载荷」为由跳过预检。

## 一、落库策略（F 载荷本地保存位置，三选一，待 dsh 裁决）

F 载荷来源 = `build/r16-fantgpu-deb/`（③ 已审计解包树：87 目录 + 602 常规文件 + 58 符号链接，deb SHA `6f0daaf7…fd11b`，逐文件 SHA 已锁定于 `docs/planning/evidence/o-stage/f-payload.manifest.tsv` 753 行）。载荷按安装布局组织（路径即包内路径）。

| 选项 | 位置 | 优点 | 缺点 | 保护区影响 |
| --- | --- | --- | --- | --- |
| **A（推荐）** | `vendor/fantgpu/` | 与 `vendor/kernel|userspace|firmware|opt` 同构；builder 复制逻辑心智一致；`.gitignore:26` 已整目录忽略 | 属保护区 `vendor/` 写入 | 需 dsh 单独授权一次性落库 |
| B | `build/r16-fantgpu-payload/` | 不碰 vendor/ | `build/` 同为保护区（.gitignore:27）且语义是「构建产物」而非「输入」，双构建 clean 时易被清理误伤 | 需 dsh 授权且易误清理 |
| C | 新顶层目录 `fpayload/`（.gitignore 新增） | 语义清晰（F 血统输入） | 引入第三种载荷目录惯例 | 新增 gitignore 行 + dsh 授权 |

推荐 **A**：manifest 的 `vendor_path` 前缀统一为 `fantgpu/`，builder 血统分支的遍历与映射自然区分两血统条目族；O 条目族（192 条）与 F 条目族完全隔离，不破坏现有 `validate-binary-manifest.py` 对 O manifest 的校验口径。

**恢复步骤（干净工作区重建输入的显式路径；codex re-review P1-1 事务化落库 + P1-2 journal 状态机 + re-review#3 P1 状态写入失败窗口 + re-review#5 P1-1 逐状态真值表 / P1-2 并发排他锁 / P2 fsync 范围）**：`tools/r16-f-payload-integrity.py` 新增 `restore` 子命令，**事务化 + 写前状态机 + 排他锁**。journal = `vendor/.fantgpu-restore.journal`（`{"state": staged|moving_old|moved_old|moving_new|committed, "pre_existing": bool, "txn": <名>, "old": <名>}`，临时文件 + `os.replace` 原子写入）。**核心纪律：先写状态、后动文件**——任何文件移动都发生在对应「意图状态」落盘之后；journal 写入本身失败时，文件尚未移动，现场可由状态机确定性解释。恢复裁决依据 = journal 状态 + `vendor/fantgpu`(C)/`txn`(T)/`old`(O) 三者**存在性** + **pre_existing 严格布尔校验** + 对拟保留目录的**内容复核**（严格清点 + SHA + mode 与 manifest 一致，即步骤 4 同一例程；不符 → fail-closed die 保留现场）。

0. **并发互斥（codex re-review#5 P1-2 / re-review#6 P1 锁生命周期定稿，先于一切扫描与暂存）**：固定锁文件 `vendor/.fantgpu-restore.lock`——`open(path, O_CREAT|O_RDWR|O_NOFOLLOW, 0o644)`（已存在则复用打开；`O_NOFOLLOW` 拒绝 symlink 劫持，open 报 ELOOP → fail-closed die），对 fd `flock(LOCK_EX|LOCK_NB)`（拿不到 → rc≠0 `restore already in progress`）；**锁文件永不删除**（/vendor/ gitignored，持久存在，避免「删除与重新获取」竞态），fd 持有至事务与恢复清理全部结束。补测试：锁文件为 symlink → fail-closed；双进程并发 → 串行化且后者不得删除前者 txn；**前一次 restore 完成后再次 restore 成功**（锁文件可复用）。

1. **deb 预检**：来源 deb SHA == manifest `source_deb_sha256`，不符 → rc≠0，不动任何文件；
2. **解包到同级临时目录**：`dpkg-deb -R <deb> vendor/.fantgpu-txn.<rand>/`（与 `vendor/fantgpu/` 同文件系统）；**此阶段无 journal**——txn 目录未经校验，但**持有排他锁**，启动扫描对 txn 的清理同样在锁内，故不存在并发互删；
3. **剔除 DEBIAN/**：`rm -rf vendor/.fantgpu-txn.<rand>/DEBIAN/`——控制成员**不属于 S_INPUT**（builder 重生成 control/postinst/md5sums），其字节锁定由 ③ 检查 C 的 S2↔S3 双向比对 + manifest `source_deb_sha256` 承载；
4. **严格清点 + SHA/mode 校验**：临时树 == S_INPUT 严格双射——恰好 87 目录 + 602 常规文件 + 58 符号链接，**零额外路径**（含零 DEBIAN/ 残留），每条 SHA/链接目标/**mode** 与 manifest 一致；任一不符 → 删除临时目录、rc≠0、旧 `vendor/fantgpu/`（若存在）原样保留；
5. **写前状态机切换**（journal 状态写失败点均被后续恢复规则覆盖）：
   - 写 `{state: staged, pre_existing, txn, old}` →（仅当 pre_existing）写 `{state: moving_old}` → `mv C → O` → 写 `{state: moved_old}` → 写 `{state: moving_new}` → `mv T → C` → 写 `{state: committed}` → `rm -rf O` → 删 journal；
   - **moving_old 写入失败**（文件未动）→ 删 T、删 journal、rc≠0，旧代零改动；
   - **old-mv 失败**（journal=moving_old，O 不存在）→ 删 T、删 journal、rc≠0，旧代仍在原位；
   - **moved_old 写入失败**（old-mv 已成功）→ journal 停在 moving_old 但 O 存在 → 按真值表走 moved_old 分支，无孤立；
   - **moving_new 写入失败**（新代未动）→ journal=moved_old（或首次落库 staged）→ 真值表覆盖；
   - **new-mv 失败**（journal=moving_new，C 不存在）→ 真值表覆盖；
   - **committed 写入失败**（new-mv 已成功）→ journal 停在 moving_new 但 C 存在 → 真值表覆盖，无混代歧义；
6. **启动扫描（每次 restore 开始前，锁内执行，幂等，逐状态存在性真值表 + 内容复核）**：
   - **journal 字段安全校验（codex re-review#4 P1-2，先于一切读取/执行）**：journal 路径本身必须是非 symlink 常规文件（symlink → fail-closed die）；JSON 可解析且 `state` ∈ 枚举且 `pre_existing` 为布尔；`txn`/`old` 字段必须匹配工具生成格式 `^\.fantgpu-(txn|old)\.[0-9a-z]{8}$`（固定前缀 + 随机名）；`os.path.realpath(dirname) == os.path.realpath(vendor)` 且 `basename == 记录值`（防路径逃逸）；**存在的候选 C/T/O 必须为非 symlink 真实目录**（symlink → fail-closed die；缺失是合法状态，由真值表解释）；字段缺失/类型不符/存在同名额外残留 → 一律保留现场 fail-closed die。**不得**因「journal 是工具写出的」假设而跳过校验——mv/rm -rf 的对象必须全部通过上述约束；
   - **逐状态合法性规则（C/T/O 为存在性，1=存在 0=缺失；下表仅列出**正向流程或恢复动作实际可产生**的组合，其余一切组合——含 txn 被外部删除、journal 被篡改产生的组合——一律保留现场 fail-closed；codex re-review#6/#7 P1 修正版）**：

| state（pre） | 可产生的合法组合（C,T,O） | 来源 | 动作 |
| --- | --- | --- | --- |
| `staged`（pre=T） | (1,1,0) | 正向：staged 写入后 | 删 T、删 journal，C 未动 |
| `staged`（pre=T） | (1,0,0) | 恢复动作残留：T 已删、journal 未删 | 删 journal |
| `staged`（pre=F） | (0,1,0) | 正向：staged 写入后 | 删 T、删 journal |
| `staged`（pre=F） | (0,0,0) | 恢复动作残留 | 删 journal |
| `moving_old`（pre=T） | (1,1,0) | 正向：mv C→O 之前 | 删 T、删 journal，C 未动 |
| `moving_old`（pre=T） | (0,1,1) | 正向：mv C→O 之后、moved_old 写入前 | 视同 `moved_old` 处置 |
| `moving_old`（pre=T） | (1,0,0) | 恢复动作残留：T 已删 | 删 journal |
| `moved_old`（pre=T） | (0,1,1) | 正向：mv T→C 之前 | 内容复核 O → mv O→C → 删 T、删 journal |
| `moved_old`（pre=T） | (1,1,0) | 恢复动作残留：O 已 mv 回 C、T 未删 | 内容复核 C → 删 T、删 journal |
| `moved_old`（pre=T） | (1,0,0) | 恢复动作残留：T 已删、journal 未删 | 内容复核 C → 删 journal |
| `moving_new`（pre=T） | (0,1,1) | 正向：mv T→C 之前 | 视同 `moved_old` 恢复 |
| `moving_new`（pre=T） | (1,0,1) | 正向：mv T→C 之后、committed 写入前 | 内容复核 C → 视同 `committed` 清理 |
| `moving_new`（pre=T） | (1,1,0) | 恢复动作残留：O 已 mv 回 C、T 未删 | 内容复核 C → 删 T、删 journal |
| `moving_new`（pre=T） | (1,0,0) | 恢复动作残留：清理完成、journal 未删 | 内容复核 C → 删 journal |
| `moving_new`（pre=F） | (0,1,0) | 正向：mv T→C 之前（新代未就位、txn 在、本无旧代） | 删 T、删 journal |
| `moving_new`（pre=F） | (0,0,0) | 恢复动作残留：T 已删 | 删 journal |
| `moving_new`（pre=F） | (1,0,0) | 正向：mv T→C 之后 / 残留 | 内容复核 C → 删 journal |
| `committed`（pre=T） | (1,0,1) | 正向：rm O 之前 | 内容复核 C → 删 O → 删 journal |
| `committed`（pre=T） | (1,0,0) | 正向：rm O 之后 / 残留 | 内容复核 C → 删 journal |
| `committed`（pre=F） | (1,0,0) | 正向 / 残留 | 内容复核 C → 删 journal |

   - **不可产生组合（一律 ✗ fail-closed，保留现场）**：pre_existing=F 出现 `moving_old`/`moved_old` 状态或任何 O=1；任何状态的 T=1 与 O=1 并存于非上表位置；`moved_old` 的 (0,1,0)（T 被外部删除）；`moving_new`(pre=T) 的 (0,1,0)、`committed` 下 C=0 或 T=1；上表未列出的任何组合；C/T/O 为 symlink 或非真目录（前文字段校验）。**「未列组合 fail-closed」由此真正成立：缺失 txn 的中间态不再被当作合法残留恢复。**
   - journal 不存在 → 删除全部 `vendor/.fantgpu-txn.*`（未校验垃圾；锁内）；**任何 `vendor/.fantgpu-old.*` → fail-closed die**（无 journal 无法证明代次，要求人工裁决）；C 存在即当前代。
   - **fsync 保证范围（codex re-review#5 P2，显式收窄）**：write-ahead 状态机的原子性保证覆盖**进程中断与并发进程**；**不承诺掉电/系统崩溃下的 durable ordering**（journal 状态写与目录 mv 之间不保证 fsync 屏障）。若未来需要掉电级保证，须在每次状态写后对 journal 文件与其父目录 fsync，并重新裁决本设计的保证范围。

落库动作（restore 或首次复制）对 `vendor/` 的写入均需 dsh 授权。

## 二、三集合建模（codex P1-1 修正：输入清单 / 锁定对照 / 物化集）

F manifest（`binary-manifest-fantgpu.json`，git 追踪）顶层 schema 同 O（`format_version: 1`、`architecture: amd64`、`source_package: fantgpu-fh2m`、`source_version: 3.3.8.126-driver-linux-desktop-sp-generic`、`source_deb_sha256: 6f0daaf79fb6b2a547138c17628bb990dff0d0c684ee1c13775bebc2d28fd11b`），条目字段在 O 基础上增加 `variant`、`materialize`、`mode` 三键（codex re-review#7 P1-2：无单值 `set` 字段）。**三个集合显式分层，各自有 validator 与覆盖计数**：

| 集合 | 定义 | 覆盖 | validator | 计数（顶层字段） |
| --- | --- | --- | --- | --- |
| **S_INPUT（完整输入清单）** | F 载荷**全部 602 常规文件 + 58 符号链接**（目录不入条目，O 先例同；DEBIAN/ 控制成员不入条目——builder 重生成 control/postinst/md5sums，其锁定由 ③ 检查 C 的 S2↔S3 双向字节比对承载） | 660 条 | `validate-binary-manifest-fantgpu.py`：每条 `vendor/fantgpu/<path>` 存在且 SHA/链接目标与 manifest 一致；输入预检（§〇）同源调用 | `input_entries=660` |
| **S_LOCKED（locked-reference-only）** | ① `usr/src/fantgpu-fh2m-kernel-2.2/**`（F deb 自带内核源，约 490 文件）——builder F 分支已自装 O_stage 树为该路径，避免冲突；② **vendor 死暂存**（厂商 postinst 亦不物化的 `/opt` 条目，见 §四 M6 分类表）：`usr/share/alsa/ucm/FantasyCard/*`（3 条）、`usr/share/alsa/ucm2/FantasyCard/*`（2 条）、`usr/sbin/sw-fant-gl` + `lib/systemd/system/sw-fant-gl.service`（2 条，不安装不启用，理由见 §四 M6） | 该两类全部条目（计数由生成器实算） | 同 S_INPUT（存在 + SHA）；builder 物化阶段**不复制** | `locked_reference_entries=<n>` |
| **S_MATERIALIZE（物化集）** | 经 `F_XORG_ABI`/`F_UCM_LAYOUT`/`F_WAYLAND_COMPAT` 选择与 materialization 规则（§四 M1-M6）后**实际进入包**的集合 | `f_materialized_entries=<n>`（F manifest 来源，builder 实测；O_stage 来源另计 `ostage_materialized_entries`，见 §四 M2 trace 计数拆分） | builder 物化后断言：每个落位文件 SHA == 对应来源清单（F manifest / o-stage.manifest.tsv）；计数 == 期望；**未选变体与 S_LOCKED 条目不得出现在 $P** | 期望值由生成器按预选参数预计算入 manifest 或 sidecar |

- **集合口径（codex re-review#7 P1-2 修正：弃单值 `set`，S_INPUT 为隐含集合）**：**S_INPUT = 所有条目**（隐含，不设字段）；S_LOCKED = `materialize: locked-reference` 的条目集合；S_MATERIALIZE = 其余条目经预选后实际物化的集合。生成器、validator、sidecar 计数统一使用同一口径（`input_entries` = 条目总数 = 660）。
- **条目 `variant` 键**（仅变体条目）：`ddx-abi-<1.19|1.20|1.21>`、`ucm-<ucm|ucm2>-<rel>`、`wayland-compat`。**每条 `/opt/fantgpu-fh2m/` 下的条目必须被分类**（§四 M6 分类表已全量给出），生成器遇未分类条目 fail-closed。
- **条目 `materialize` 键**：`direct`（默认，`fantgpu/<p>` → `$P/<p>`）；`locked-reference`（不复制）；`ddx-abi-<x>`（仅当 `F_XORG_ABI=x` 时按 M3 变换）；`ucm-<layout>-<rel>`（仅当选定时按 M4 变换）；`wayland-compat`（仅当 `F_WAYLAND_COMPAT=on` 时按 M5 变换）。
- **符号链接条目**：`sha256/size` 为空、`target` 键记录链接目标；物化用 `ln -sfn "<target>" "$P/<p>"`（F 载荷链接目标均为载荷根内相对路径，③ E 检查已证全树零悬空，物化后有效性由 builder 断言复验：每个落位链接的 `readlink -f` 解析到包内常规文件）。
- **条目字段补充（codex re-review#5 P1-3：mode 锁定）**：常规文件条目增加 `mode`（4 位八进制，取自 ③ `f-payload.manifest.tsv` mode 列）；符号链接条目记录 `target` + `mode`（Linux 符号链接 mode 恒 0777，记录仅作一致性锁定）；目录不入条目但 restore/预检按 **固定目录 mode 0755** 校验。可复现性声明因此同时锁定**内容（SHA）、布局（路径）、类型（file/link/dir）、权限（mode）**——chmod 漂移不改 SHA 但改变 deb 字节，必须被检出。
- **生成工具** `tools/gen-fantgpu-manifest.py`：读 ③ `f-payload.manifest.tsv` → 生成 manifest + sidecar 计数（`input_entries`/`locked_reference_entries`/按预选参数的 **`f_materialized_entries`** 期望值 + **`f_dir_entries=87`**；O_stage 侧计数 `ostage_materialized_entries`/`ostage_dir_entries=16` 由 builder 侧常量断言，不入 F sidecar）；校验 S_INPUT 与 ③ 清单**严格双射**（零多写/漏写，含 mode 逐值）、变体组完备（每个 `/opt` 条目已分类、每个 ABI/UCM 组非空）。

## 三、builder 8 点改造（`scripts/build-innogpu-driver.sh`，FANT_LINEAGE=1 分支）

| # | 位置（现状） | 改法 |
| --- | --- | --- |
| 1 | `:238` 载荷遍历只读 `binary-manifest.json` | FANT_LINEAGE=1 → **输入预检**（§〇）后按 `binary-manifest-fantgpu.json` 物化；条目 `vendor_path` 前缀 `fantgpu/` |
| 2 | `:229` `firmware/*) dst="$P/lib/firmware/innogpu/${vp#firmware/}"` | F 分支按 §四 M1-M6 规则物化（**不再复用** O 的 case 映射）；`firmware/*` O case 保留给 O 血统 |
| 3 | `:216-219` `kernel/*` F 血统跳过 | 保持（O_stage 树自带 `fant*.o_shipped`） |
| 4 | `:352-355` postinst 硬要求 `dri/innogpu_dri.so` | F 分支断言改 `dri/fh2m_dri.so`（参数化） |
| 5 | `:257` ld.so.conf 固定 `innogpu-fh2m` | F 分支写 `/usr/lib/x86_64-linux-gnu/fantgpu-fh2m`（F 包实测私有库目录） |
| 6 | `:194`/`:264` share 目录固定 `innogpu-fh2m-trixie` | 按血统参数化（`fantgpu-fh2m-trixie`）；与 C1-① 联动（helper 比对路径） |
| 7 | `:266-277` 命令 symlink 固定 `innogpu-*` | 按血统参数化（`fantgpu-*`）；与 C1-① 联动 |
| 8 | F 包额外载荷未纳入 | 已含于 S_INPUT（`etc/modprobe.d/blacklist-fh2m.conf`、`usr/share/X11/xorg.conf.d/10-fh2m.conf`、`fh2m_conf.json`/`00_fh2m.json`/`FANT_fh2m.icd`/`01-fh2m_drv.conf` 等）→ M1 直接物化；`usr/src/fantgpu-fh2m-kernel-2.2/**` 归 S_LOCKED 不装；DDX/UCM/wayland 变体按 M3-M5 |
| 9 | `:11-21` 版本 allowlist 精确写死 `5.0.0-i1` | 新增 `5.0.0-i2) EXPECTED_SOURCE_DATE_EPOCH=1788796800`（沿用 dsh 已审核 epoch，o-stage-integration-plan §二 口径）；版本排序门 `:50` 自动覆盖 |
| 10 | `:24-25` FANT_LINEAGE 判定精确写死 `== 5.0.0-i1` | 改 `[[ "$VERSION" == 5.0.0-i* ]]`（5.0.0-iN 系列统一 fantgpu 血统；其余仍 innogpu） |
| 11 | `:102-105` 输入预检无条件检查 O manifest + O vendor | **按血统分支**：F 分支只要求 `binary-manifest-fantgpu.json` + `vendor/fantgpu/` 输入预检（§〇，含 SHA+mode+链接目标+严格双射），**不要求** O manifest/O vendor 就位；O 分支保持现状 |
| 12 | F 分支 DESC_BODY 文案「coherent Deepin userspace payload」+ **PKG_DESC（builder `:283`）写死「fantgpu lineage 5.0.0-i1, O_stage materialized tree」** | DESC_BODY 改「coherent fantgpu (F) userspace payload（binary-manifest-fantgpu.json 锁定；构建期预选 DDX/UCM/wayland）」；**PKG_DESC 版本串参数化 `fantgpu lineage $VERSION, O_stage materialized tree`**（不得写死 i1/i2）；新增断言：构建后 control 的 Description 必须含 `$VERSION`（builder 单测覆盖 i2 场景） |
| 13 | `5.0.0-i1.meta.json` 引用 | **保持不变**（materialize 五件不变 → OSTAGE_SNAP/meta 路径与文件名固定，i2 继续使用同一物化产物与同一 meta；不新增 `5.0.0-i2.meta.json`） |

另：`:415` `release-audit gate pending adaptation` 跳过行，在 **C1-② 同批**移除（§六 时序）。

## 四、materialization 规则与强制项

**M1 直接物化（默认）**：`materialize=direct` 条目 `fantgpu/<p>` → `$P/<p>`；常规文件 `install -m <mode>`（**显式 mode，不得依赖 cp/umask**；builder 物化前显式 `umask 022` 并固定）；符号链接 `ln -sfn "<target>"`；`mkdir -p` 父目录（目录 mode 0755）。**校验**：落位 SHA + mode == manifest 条目。

**M2 锁定对照**：`materialize=locked-reference` 条目参与输入预检（存在 + SHA + mode）但**不复制**；因 O_stage 与 F deb 使用**相同安装路径** `usr/src/fantgpu-fh2m-kernel-2.2`，最终路径无法区分来源——断言改为：① 最终 subtree **内容 hash == O_stage 解包预期**（builder 既有 O_stage 快照 SHA + 解包树 hash 校验）；② **物化 trace 契约（codex re-review#7 P2 / re-review#8 P1-2 计数拆分 / re-review#9 P1 类型语义 / re-review#10 P1 O_stage mode 来源修正：机器可判定的最小记录）**：builder 物化每写入一个**载荷落位**，向 `$STAGE/materialize-trace.tsv` 追加一行 **四列** `source_entry<TAB>destination<TAB>rule<TAB>kind`（source_entry = 来源清单路径：F 载荷 = manifest `vendor_path`（`fantgpu/…`）、O_stage 内核 = `o-stage.manifest.tsv` 相对路径；destination = `$P` 相对路径；rule ∈ `direct|ddx-abi-<x>|ucm-<layout>|wayland-compat|ostage-kernel`；**kind ∈ `regular|symlink`**）。**trace 只记录常规文件与符号链接，目录不入 trace**——目录由独立断言覆盖：**目录计数**（`f_dir_entries=87`、`ostage_dir_entries=16`）+ **目录 mode 固定 0755** + O_stage subtree hash（上①）。**计数拆分（不再使用单一 materialized_files）**：`f_materialized_entries`（F manifest 来源物化 regular+symlink 条目数，期望值由生成器预计算）、`ostage_materialized_entries`（O_stage 来源物化 regular 条目数；F0 无 symlink）、`trace_entries = f_materialized_entries + ostage_materialized_entries`。**分 kind、分源校验（codex re-review#10 P1：O_stage mode 来源 = 锁定快照，不在清单内）**：`kind=regular` 且 rule=fantgpu-* 行 → destination **SHA 与 mode** 均与 F manifest 一致（F manifest 有 mode 列）；`kind=regular` 且 rule=ostage-kernel 行 → destination **仅校验 SHA**（与 `o-stage.manifest.tsv` 的 `file_sha256` 一致）——**O_stage 的 mode 由锁定快照字节承载**（`o-stage-snapshot.tar.zst` SHA 已锁于不可变 meta，materialize 五件不可变 → 不能也不需为 o-stage.manifest.tsv 增补 mode 列；builder 解包即得锁定 mode，双构建字节一致由既有证据覆盖）；`kind=symlink` 行 → destination 链接目标与来源清单 `target` 一致（无 SHA 概念；仅 F 来源有 symlink）。**builder 生成物（helper 脚本、DEBIAN/control/postinst/md5sums 等）不入 trace**（非载荷物化，由 builder 生成逻辑与 md5sums 覆盖）。**校验（builder 断言 + 单测）**：trace 不得出现任何 `materialize=locked-reference` 的 source_entry（S_LOCKED 内核源条目出现 → `builder_materialize_trace=FAIL`）；trace 行数 == `trace_entries`；每行 destination 在 `$P` 内存在且按 kind/来源校验一致。生命周期：仅构建期产物（STAGE 内），不进包、不入 git。

**M3 DDX 预选**：三个 ABI 条目 `materialize=ddx-abi-<x>`；仅 `F_XORG_ABI`（默认 `1.21`）命中条目物化：`vendor/fantgpu/opt/fantgpu-fh2m/usr/lib/xorg/modules/drivers/fh2m_drv.so.<x>` → `$P/usr/lib/xorg/modules/drivers/fh2m_drv.so`（**去版本后缀、去 staging 前缀**）；未选两 ABI 不进入包；`$P/opt/fantgpu-fh2m/` 下不残留任何 DDX 文件。

**M4 UCM 预选**：条目 `materialize=ucm-<layout>-<rel>`；仅 `F_UCM_LAYOUT`（默认 `ucm2`）命中组物化：`fantgpu/opt/fantgpu-fh2m/usr/share/alsa/<layout>/<rel>` → `$P/usr/share/alsa/<layout>/<rel>`（去 staging 前缀）；未选组不进入包。

**M5 wayland 兼容库**：条目 `materialize=wayland-compat`；`F_WAYLAND_COMPAT=on` 时物化：`fantgpu/opt/fantgpu-fh2m/usr/lib/<triplet>/fantgpu-fh2m/<rel>` → `$P/usr/lib/<triplet>/fantgpu-fh2m/<rel>`（去 staging 前缀）；`off`（默认）时不进入包。

**M6 `/opt` 静态条目（codex re-review P1-2 闭合：最终目标路径已定，不再待裁）**：非变体组的 `/opt/fantgpu-fh2m/` 条目按下列分类表**固定物化**（口径 = 与厂商 postinst 实际复制行为一致，**去 staging 前缀装 `/usr`/`/lib`，不保留 `/opt` 原位**；厂商死暂存归 S_LOCKED）：

| `/opt/fantgpu-fh2m/` 下条目 | 分类（variant/materialize） | 最终包内路径 | 依据（厂商 postinst 行为） |
| --- | --- | --- | --- |
| `usr/lib/xorg/modules/drivers/fh2m_drv.so.1.19/.1.20/.1.21`（3） | `ddx-abi-<x>`；仅 `F_XORG_ABI` 命中 | `/usr/lib/xorg/modules/drivers/fh2m_drv.so` | `drv_install :122-136`（构建期预选替代安装期探测） |
| `usr/share/alsa/ucm2/conf.d/FantasyCard/{FantasyCard.conf,HiFi.conf}`（2） | `ucm-ucm2-conf.d-<rel>`；`F_UCM_LAYOUT=ucm2` 时物化 | `/usr/share/alsa/ucm2/conf.d/FantasyCard/<rel>` | `ucm_install :94-96`（is_ucm2=true → 只复制 conf.d/FantasyCard） |
| `usr/share/alsa/ucm/FantasyCard0-*`（44 条，含子目录 conf 与 Dp/Hdmi 状态文件） | `ucm-ucm-<rel>`；`F_UCM_LAYOUT=ucm` 时物化 | `/usr/share/alsa/ucm/FantasyCard0-*/<rel>` | `ucm_install :98-99`（is_ucm2=false → `FantasyCard0*` 全量） |
| `usr/share/alsa/ucm/FantasyCard/*`（3）与 `usr/share/alsa/ucm2/FantasyCard/*`（2） | `materialize=locked-reference`（vendor 死暂存） | **不进入包** | 厂商 postinst 两分支均不复制（非 conf.d 的 ucm2 变体与 ucm/FantasyCard 从未被 `cp`） |
| `usr/lib/{x86_64,i386}-linux-gnu/fantgpu-fh2m/{libwayland-client,libffi}.so*`（12） | `wayland-compat`；`F_WAYLAND_COMPAT=on` 时物化 | `/usr/lib/<triplet>/fantgpu-fh2m/<rel>` | `update_wayland :29-48`（trixie `libwayland-client0` 实测 1.23.1 ≥ 1.20 → 默认 off 正确） |
| `usr/sbin/sw-fant-gl`（1） | `materialize=locked-reference`（**不安装、不启用**） | **不进入包** | 脚本依赖 `/etc/.fantgpu.cfg`（:56/:95）与不存在的 `BINARYDIR=/opt/fantgpu/usr/local/bin/`（:7）；实测其 `update` 唯一实质行为 = 写 `0-fantgpu.conf` + ldconfig（add_binary/add_dri_link 因 vendor cfg 指向不存在路径/占位 token `DRI_LINK` 实为 no-op），该行为由包内静态 `0-fantgpu-hwgl.conf`（builder 改造点 #5）**等价覆盖**。二选一裁决：**不安装**（避免运行依赖已删 `/opt` 与 cfg 的矛盾；O 血统 4.0.2-i3 无此服务且真机可用为对照基线） |
| `lib/systemd/system/sw-fant-gl.service`（1） | `materialize=locked-reference` | **不进入包**（无启用链接） | 同上 |

- 分类表之外不允许存在任何 `/opt/fantgpu-fh2m/` 条目（生成器 fail-closed）；**物化后 `$P/opt` 必须为空**（去 staging 前缀口径下 `/opt` 不再存在）。**变换物化（M3-M5）与 M1 同口径：以 manifest 条目 mode 显式落位（`install -m`），umask 022 固定**。
- `F_XORG_ABI` 默认 `1.21` 的技术依据：trixie `xserver-xorg-core 21.1` 的视频驱动 ABI（25.2）与 xserver 1.21 同族，`fh2m_drv.so.1.21` 为正确变体；厂商 `drv_install` 按 `Xorg -version` 前两位（`21.1`）寻名会**找不到任何变体**（变体只有 1.19/1.20/1.21）——构建期预选恰好修复该上游缺陷。真机批 R1 观察 Xorg 实际加载行为复核。

**物化后总断言（builder）**：`f_materialized_entries`/`ostage_materialized_entries`/`trace_entries` 计数 == 期望值（§四 M2 拆分口径）；`find $P -type f` 中来自 F 载荷的每个文件 SHA 命中 F manifest 且**反向无遗漏**（未选变体/S_LOCKED 零出现）；**`$P/opt` 必须为空**（去 staging 前缀口径）。

**强制项 1：md5sums 按实际载荷重生成（确定性契约，codex P1-3 修正）**
- 新工具 `tools/gen-package-md5sums.py`（F 分支专用；O 分支行为不变）：`LC_ALL=C`；`os.walk(followlinks=False)` 收集 `$P` 下**常规文件**（**符号链接排除**，与 dpkg 惯例一致），排除 `DEBIAN/` 子树；相对路径（去 `$P/` 前缀、`/` 分隔）；**按 LC_ALL=C 字节序排序**；每行 `<md5>  <relpath>\n`（双空格，Debian 惯例）；尾换行。单测：双跑字节一致、排序锁定、symlink/DEBIAN 排除、路径规范化。
- **双构建断言**：build-A/B 的 `DEBIAN/md5sums` 逐字节一致（整体 deb 字节一致已含此项，另加显式 `cmp` 证据行）。

**强制项 2：安装期探测改构建期决定 + postinst 行为精确固定（codex re-review P1-2 闭合）**
- 采用 validation-plan §三 已建议的 **方案 (a)：构建期预选、直接安装到最终路径**（即 M3-M6 分类表）；预选参数默认值已按真机目标核实：`F_XORG_ABI=1.21`（xserver 21.1 视频 ABI 25.2 与 1.21 同族）、`F_UCM_LAYOUT=ucm2`（trixie 实测存在 `/usr/share/alsa/ucm2`）、`F_WAYLAND_COMPAT=off`（trixie `libwayland-client0` 实测 1.23.1 ≥ 1.20）。
- F 分支 postinst（builder 生成，**弃用厂商探测式 postinst 全部逻辑**），行为逐项固定如下：

| 厂商 postinst 行为（出处） | 本包 postinst 定案 |
| --- | --- |
| `rm -r /etc/ld.so.cache`（:4-6） | **弃用**（`ldconfig` 自会重建缓存） |
| `sed -i /usr/bin/Xorg`（:8-15） | **弃用**（trixie 无 `fant_basedir`；不得改系统二进制） |
| `i2c-algo-bit >> /etc/modules`（:27） | **弃用**（不得改 `/etc/modules`；O 先例无此行为） |
| `update_wayland`（:29-48） | **弃用**（构建期 `F_WAYLAND_COMPAT` 决定，M5） |
| `ucm_install`（:64-120） | **弃用**（构建期 `F_UCM_LAYOUT` 决定，M4） |
| `drv_install`（:122-136） | **弃用**（构建期 `F_XORG_ABI` 决定，M3） |
| `update_swgl`（:139-150）+ `Link` 启用（:167-168） | **弃用且不安装**：`sw-fant-gl` 与服务 unit 归 S_LOCKED（不进入包、无启用链接），理由见 M6 分类表——脚本依赖已删 `/opt` 与 cfg，其唯一实质行为由包内静态 `0-fantgpu-hwgl.conf` 等价覆盖 |
| 写 `/etc/.fantgpu.cfg`（:153-156） | **弃用**（其引用含不存在的 `/opt/.../usr/local/bin/`；包内无已知消费者；O 先例不写 cfg。真机批观察项：若 R 项发现消费者再回补） |
| kylin/uos 分支（:17-25/:172-176/:197-209） | **弃用**（目标仅 trixie） |
| 临时 `blacklist-fantgpu.conf` + `dkms_configure`（:211-229） | **弃用**；改 O 分支先例：`dkms add/build/install -m fantgpu-fh2m-kernel -v 2.2`（payload 内的 `usr/share/fantgpu-fh2m-kernel-dkms/postinst` 按 M1 落位、仅作参考不调用） |
| `ldconfig`、`depmod -a <kernel_ver>`、`update-initramfs -u -k <kernel_ver>`（:177-186） | **保留**（单内核版本，同 O 分支 postinst 口径；弃用 `-k all` 与 dracut 分支） |
| **设备门**（厂商无；③ G 检查锁定 PCI ID） | **新增显式设备门**：`lspci -n -d 1ec8:9810 | grep -q .` 失败 → `WARNING: no 1ec8:9810 device present` **不中止**（安装器在无设备机可完成构建/测试场景；真机批步骤 3 的设备存在性由 R 项权威判定）；断言 `/usr/lib/x86_64-linux-gnu/dri/fh2m_dri.so` 存在（builder 改造点 #4 参数化） |

- 确定性（codex re-review#7 P2 文案修正）：postinst **无任何安装期载荷选择/路径选择分支**（dkms 版本路径除外——与 O 先例同）；设备缺失 warning（`lspci -n -d 1ec8:9810`）属允许的安装期检测，不构成载荷选择。
- **prerm/postrm 清理契约（codex re-review P1-1 要求纳入）**：本包**不创建任何不由 dpkg 管理的文件/链接**——服务未安装（无 `/etc/systemd` 启用链接需清理）、`/etc/.fantgpu.cfg` 不写、静态 conf/黑名单/DRI 链接均为包内文件由 dpkg 自动移除；prerm（remove/upgrade/deconfigure）= `dkms remove -m fantgpu-fh2m-kernel -v 2.2 --all`（O 先例口径）；postrm（remove/purge）= 空操作 + 与 O 先例相同的 ldconfig 刷新（`ldconfig` 幂等）。系统级副作用仅 `update-initramfs`/`depmod`（O 先例同口径，真机回退由受监督快照恢复保证）。

## 五、重构建与 provenance 传导（§二 契约）

- **版本**：载荷实质变化 → 建议 **`5.0.0-i2`**（待 dsh 定；若维持 5.0.0-i1 则须重出同版本新 deb 并替换 build 证据，不推荐）。builder 版本分支同 5.0.0-i1 逻辑；
- **双构建字节一致**：clean 双跑（build-A/B）→ SHA 一致 + `fantgpu.ko` vermagic PASS + **DEBIAN/md5sums 逐字节一致** → 新证据 `build-5.0.0-i2.sha256`，与 `build-5.0.0-i1.sha256` **并存不覆盖**；
- **materialize 五件产物与 `5.0.0-i1.meta.json` 不变**（本设计不动内核源树；`o_stage_tree_hash 937e3710…` 不变）→ 无需重跑 materialize、无需参数化 `materialize-o-stage.sh`；
- **validation-results 的包 provenance**：真机批实际安装的 `package_version=5.0.0-i2` + `package_deb_sha256=<新 deb SHA>` + build 证据引用；`materialize_meta_sha256` 保持绑定现有 meta 不变；
- **文档同步**：`o-stage-integration-plan.md:77` 载荷边界改为「F 血统 userspace 载荷 + 固件（`binary-manifest-fantgpu.json` 锁定；构建期预选 DDX/UCM/wayland）」；`5.0.0-i1-validation-plan.md` ④ 状态更新。

## 六、测试、门禁与提交批时序

- **静态测试（新增/扩展，全合成夹具）**：
  - `gen-fantgpu-manifest.py` 单测：S_INPUT 与 ③ 清单严格双射（含 mode 逐值）、零多写/漏写、变体组完备（未分类 `/opt` 条目拒绝）、symlink 条目口径、schema 违规拒绝、sidecar 计数正确；
  - `validate-binary-manifest-fantgpu.py` 单测：SHA 漂移检出、**mode 漂移检出**（chmod 不改 SHA 但必须 fail）、链接目标漂移检出、O 血统 loader 文件名禁则、载荷缺失检出（fail-closed 预检路径）；
  - `gen-package-md5sums.py` 单测：双跑字节一致、LC_ALL=C 排序锁定、symlink/DEBIAN 排除、相对路径规范化；
  - builder F 分支扩展单测（fake root）：fh2m 键名族断言（`dri/fh2m_dri.so`、`lib/firmware/fantgpu/fh2m/*`、`fantgpu-fh2m-trixie` share、`fantgpu-*` 命令、ld.so.conf `fantgpu-fh2m`）、M3-M5 预选（单 ABI/单 UCM 布局/wayland 默认 off）、**S_LOCKED 零进入 $P**（`usr/src/fantgpu-fh2m-kernel-2.2/**`、死暂存 UCM、`sw-fant-gl`/`sw-fant-gl.service`）、`$P/opt` 为空、**计数三分断言**（`f_materialized_entries`/`ostage_materialized_entries`/`trace_entries` == 期望 + 目录计数 `f_dir_entries=87`/`ostage_dir_entries=16` + 目录 mode 0755）、**落位 mode == 来源清单**（F 来源，含变换物化；O_stage 来源仅 SHA + 快照承载 mode）、**materialize-trace 契约**（四列、行数==trace_entries、fantgpu-* regular 行 SHA+mode 与 F manifest 一致、ostage-kernel 行仅 SHA 与 o-stage.manifest.tsv 一致、symlink 行 target 一致、目录零入 trace、locked-reference 来源出现 → FAIL、builder 生成物不入 trace）、md5sums 行数 == 实际载荷常规文件数、postinst 无任何安装期载荷选择/路径选择分支（静态审查）、**F 分支不要求 O manifest/O vendor 就位（仅 F 输入）**、**5.0.0-i2 版本 allowlist/血统判定/epoch 门**（负向：未审核版本拒绝）、**PKG_DESC 含 $VERSION**；
  - `restore` 子命令单测（journal 写前状态机全场景，故障注入）：deb SHA 预检失败拒绝且旧代原样；解包树含额外路径（含 DEBIAN/ 未剔除模拟）拒绝且旧代原样；SHA 漂移拒绝且旧代原样；**逐状态真值表全组合**（每个 state×pre_existing×(C,T,O) 组合的可产生合法/不可产生非法判定——含 **moving_new(pre=F) 正向 (0,1,0) 锁定**、moving_new(pre=F) 的 O=1、moved_old(0,1,0)（txn 被外部删除）、committed 的 T=1/C=0、pre=F 出现 moving_old/moved_old、C/T/O 为 symlink 等非法组合一律 fail-closed 保留现场）；**双进程竞争**（A 慢解包 + B 并发 restore → B 阻塞或 rc≠0，绝不删除 A 的 txn；锁串行化）；**锁生命周期**（锁文件为 symlink → O_NOFOLLOW fail-closed；前一次 restore 完成后再次 restore 成功（锁文件复用、永不删除））；**journal.moving_old 写入失败**（文件未动，旧代零改动）；**journal.moved_old 写入失败**（old-mv 已成功 → 恢复走 moving_old+old 存在分支，无孤立）；**journal.moving_new 写入失败**（新代未动 → 按 moved_old/staged 恢复）；**journal.committed 写入失败**（new-mv 已成功 → 恢复走 moving_new+新代存在分支，无混代歧义）；**首次落库第二次 mv 失败**（moving_new + pre_existing=false）→ 无当前代、零残留、可重跑；**两 mv 之间中断** → 旧代恢复；**旧代恢复完成但 journal 删除失败**（moved_old + `.old` 缺失 + `vendor/fantgpu` 存在）→ 下次启动幂等清理，不误判损坏（codex re-review#4 P1-1）；**新代就位但 old 清理失败**（committed + old 残留）→ 下次启动清理；**journal 字段安全**（journal 为 symlink / `txn`·`old` 字段格式非法（路径逃逸、非工具生成名）/ 被引用目录为 symlink / 字段篡改 / 同名额外残留）→ 一律 fail-closed die 保留现场（codex re-review#4 P1-2）；**多个 .old / 无 journal 出现 .old** → fail-closed die；**多个 txn / journal=staged 残留** → 安全清理；**内容复核失败**（拟保留目录 SHA/mode 漂移）→ fail-closed die 保留现场；**mode 漂移**（chmod 不改 SHA）→ 检出 fail；成功落库后 manifest 校验通过；
- **门禁**：check-docs / validate-collab / r16-gate / git diff --check + 上述新单测 + 双构建证据；C1-①（包名白名单/版本正则/helper 路径参数化）与 ④ 同批；**C1-②（required 载荷断言按 F 路径）与 builder `:415` 门禁恢复调用在 ④ 落地 + 重构建之后同批交付**（避免长期已知红门禁，validation-plan §三 C1 时序）；
- **提交批建议**（各批独立 codex 初审 + dsh 终审）：
  1. 落库（dsh 授权后）+ `binary-manifest-fantgpu.json` + `gen-fantgpu-manifest.py`/`validate-binary-manifest-fantgpu.py`/`gen-package-md5sums.py` + `restore` 子命令 + 各自单测；
  2. builder 8 点改造 + M1-M6 物化规则 + 输入预检 + postinst F 分支重写 + builder F 分支单测；
  3. 重构建 + 双构建字节一致证据（含 md5sums cmp）+ provenance/文档同步 + C1-①（C1-② 与门禁恢复随 3 或其后）。

## 七、待裁决项（供三方定案）

1. **落库位置与保护区授权**（§一）：推荐 A（`vendor/fantgpu/`）；落库/恢复动作 = 保护区写入，需 dsh 单独授权一次性执行（restore 事务化流程见 §一）。
2. **版本号**：推荐 `5.0.0-i2`（§五）。
3. **构建期预选默认值确认**：`F_XORG_ABI=1.21`（xserver 21.1 视频 ABI 25.2 同族）、`F_UCM_LAYOUT=ucm2`（trixie 实测存在）、`F_WAYLAND_COMPAT=off`（trixie `libwayland-client0` 1.23.1）——三者已按真机目标核实，请 dsh 确认定案。
4. **share 目录/命令名血统参数化**（§三 6/7）与 C1-① 的断言口径联动——validation-plan 权威映射表已标注「⚠ 包内 share 目录 / 命令名 / ld.so.conf 仍为 innogpu 键（builder 未分支）」，本设计将其改血统后，C1-① 必须按**改后实际内容**断言。

（M6 `/opt` 分类表与 postinst 逐项行为已在 v3/v4 固定，见 §四；`sw-fant-gl` 二选一裁决 = **不安装、不启用**（S_LOCKED + 静态 conf 等价覆盖），prerm/postrm 清理契约已纳入 §四；不再待裁。）

## 八、禁止动作（本轮）

- 未获 dsh 授权前：不写 `vendor/`、不动 `binary-manifest.json`、不改 builder；
- 不重跑 materialize、不碰五件产物与 meta；
- 不交付 C1-②、不恢复 `:415` 门禁调用（时序见 §六）、不设 F7 默认身份串；
- 不放行真机批；不 commit/push/tag（dsh 执行）。
