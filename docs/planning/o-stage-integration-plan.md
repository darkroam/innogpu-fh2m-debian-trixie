# O_stage 构建集成方案（5.0.0-i1 系列）——起草稿

- 日期：2026-09-10
- 状态：**已实现**（scripts/materialize-o-stage.sh + tests/unit/run-o-stage-materialize-tests.sh + tests/unit/run-030-meta-tests.sh 已交付；O_stage 物化实跑完成，五件产物落 evidence/o-stage/；builder 集成待物化验收后单独放行——本方案 §三 builder 部分仍为设计稿）
- 放行依据：dsh 放行规格（已记 report.md）；约束不变（保护区零写入、debs/F0 只读、builder 不安装不重启）
- 前置事实：030 重放链 13 项闭合，O_stage 树 hash = `937e37107f692712e6fba9b5eb93a48dd6bb034f138e5e51a2bb9c2beaa0d652`（= 030-029 after_tree_hash）

## 一、materialize-o-stage.sh 设计（编排物化）

**目标**：从 O-4 只读输入可复现地物化 O_stage 源树快照，作为 5.0.0-i1 builder 的 DKMS 源根。

**流程**（脚本化，每步校验、fail-closed）：

1. 前置校验：tar 1.35 / zstd 1.5.7（工具版本锁定，同 O-4 先例）；输入 `docs/planning/evidence/o-stage/f0-snapshot.tar.zst` SHA == `65fa17ec…`；13 条 `patches/030-*.patch` 的 SHA 与各自 meta.json 的 `apply.patch_sha256` 一致；
2. 解包 F0 快照至事务工作目录（排他锁 + .txn 目录模式，同 O-4 事务先例），校验解包树 hash == `7219d817…`（O-4 锁定值）；
3. 按链序应用 13 条 030-NNN（`patch --batch --forward --fuzz=0 --no-backup-if-mismatch -p1`）：
   001 → 002 → 006 → 009 → 007 → 023 → 025 → 024 → 026 → 027 → 026-lifecycle → 028 → 029；
   每条应用后校验树 hash == 该条 meta.json 的 `apply.after_tree_hash`（逐链点校验，链基 = 前一条 after）；
4. 最终校验：树 hash == `937e3710…`；
5. 快照产物（可执行形式，等价于 O-4 `tools/o4-f0-lock-gen.py:235` 的参数数组）：
   ```
   tar --sort=name --mtime=@1640995200 --owner=0 --group=0 --numeric-owner \
       --no-acls --no-xattrs --no-selinux \
       --transform='s,^\.,o-stage,' -C "$O_STAGE_ROOT" -cf - . |
     zstd -q -19 > "$TXN/o-stage-snapshot.tar.zst"
   ```
   （`--transform` 参数必须单引号包裹防止 shell 吞掉 `^\.` 反斜杠；`-C "$O_STAGE_ROOT" -cf - .` 指定归档输入根；产物先写入事务目录，见事务语义）
6. 记录产物（**materialize 事务产物 = 五件同代**，与提交语义一致）：
   - `o-stage-snapshot.tar.zst` + `.sha256`
   - `o-stage.manifest.tsv` + `.sha256`
   - `5.0.0-i1.meta.json`：**不可变 provenance**（版本串 5.0.0-i1 / SOURCE_DATE_EPOCH（批准值）/ tag 名 fantgpu-5.0.0-i1 / O_stage 树 hash 937e3710… / 13 条 030-NNN 链（各 patch SHA + after_tree_hash）/ 两条 UNVERIFIED 登记 / **快照四文件各自 SHA-256**（o-stage-snapshot.tar.zst、o-stage-snapshot.tar.zst.sha256、o-stage.manifest.tsv、o-stage.manifest.tsv.sha256））——materialize 时一次性写完，**此后不再修改**；验证矩阵结论**不写入本文件**（见下）

**meta.json 生命周期（消除同代冲突）**：

- materialize 阶段：`5.0.0-i1.meta.json` 作为 provenance 在事务内生成，全部字段在 materialize 时已知（含快照四文件 SHA），随五件同代产物统一提交；此后不可变（任何字段变更 = 新的受控事务 + 全套产物重生成/重校验，不复用旧 sidecar）；
- 验证阶段：验证矩阵结论写入**独立文件** `docs/planning/evidence/o-stage/5.0.0-i1-validation-results.json`（不属于 materialize 同代产物集；内容 = 阶段三矩阵逐项结论 + 签发前全 PASS 状态）；签发前提 = 该文件全 PASS；
- 两者绑定关系（**单向绑定，validation 锚定 immutable meta**）：
  - meta.json 固定记录 validation-results 文件路径与 schema（meta 只引用文件名，不引用其哈希——validation 后生成，其哈希在 materialize 时不可知）；
  - validation-results 记录 `materialize_meta_sha256`（= 5.0.0-i1.meta.json 的 SHA-256，验证阶段校验一致后方可填充结论）；
  - validation-results 自身完整性由最终 Git commit / annotated tag（fantgpu-5.0.0-i1）或独立 validation sidecar 锚定——不做「互引 SHA-256」，因为 mutual 引用在生成时序上不可实现（meta 先于 validation 固化）。

**fail-closed**：链中任一 patch 失败（rc≠0 / .orig/.rej / 链点 hash 不符）→ 清理事务目录、不产任何快照产物、非零退出。

**manifest 与 tree hash 契约（复用 O-4 契约，逐条列明）**：

- 四字段格式：`type<TAB>relative_path<TAB>symlink_target<TAB>file_sha256<LF>`；type ∈ {d 目录, f 文件, l 符号链接}；首行为根目录行 `d\t\t\t\n`；
- 排序：LC_ALL=C 字节序（按 type+path 字典序，O-4 `walk_rows` 的实现口径）；
- 路径：相对路径以 `/` 分隔；control char 拒绝（NUL 等，O-4 `check_no_ctrl` 口径）；
- **tree hash = canonical manifest 字节（LF 序列化）的 SHA-256**（非文件系统遍历散列）；链点校验与最终校验均用此定义；
- 快照解包后 reconcile：解包 `o-stage-snapshot.tar.zst` → 重新 walk_rows 生成 manifest → 与 `o-stage.manifest.tsv` 逐行一致 → 其 SHA-256 与 `o-stage.manifest.tsv.sha256` 一致（O-4 `cmd_verify` 口径）；
- 产物侧文件：`o-stage-snapshot.tar.zst.sha256` 与 `o-stage.manifest.tsv.sha256` 双 sidecar。

**事务与提交语义（fail-closed 可执行契约）**：

- 输出目录 canonical path 校验（realpath + commonpath 边界），symlink/path escape 拒绝（O-4 `check_out_dir` 口径）；
- 排他锁在任何输出变更前取得（O-4 `flock` 口径）；
- 所有产物（**五件完整：tar + 快照 sidecar + manifest + manifest sidecar + 5.0.0-i1.meta.json**）先写入同一事务目录并完成内部校验（tree hash / sha256 一致性）；**生成顺序**：先产 tar/manifest 及各自 sidecar，再生成引用其 SHA 的 meta.json，全部校验通过后统一提交；
- 提交点：全部校验通过后按序原子替换到最终位置；journal 记录状态机（staged/committed/rolling_back），每步 fsync（文件 + 目录）；
- 失败/中断：原有完整产物保持不变；恢复入口按 journal 状态幂等续跑（O-4 `recover_interrupted` 口径）；幂等重跑规则 = 相同输入重跑产出字节一致的产物；
- **禁止混代**：任何状态下不允许新 tar + 旧 manifest（或反向）共存于最终位置——产物对（tar+sha256+manifest+sha256+meta）要么全部同代、要么全部保持旧代。

**快照参数先例**：tar 1.35 / zstd 1.5.7 / @1640995200 / --sort=name / owner·group 0 / transform 前缀 `o-stage`——与 O-4（前缀 f0）与 D_stage 先例完全同构，仅前缀不同。

## 二、版本与 epoch 定稿提案（待 dsh/用户批准）

| 字段 | 提案 | 理由 |
| --- | --- | --- |
| 版本串 | `5.0.0-i1` | fantgpu 血统新系列（Deepin 血统为 4.0.2-i3）；dpkg 版本字段 5.0.0-i1 > 4.0.2-i3，升级序天然正确，无需 deb Epoch 字段（保持缺省） |
| tag 名 | `fantgpu-5.0.0-i1`（dsh 唯一确认，仅此名） | 与 Deepin 血统 `deepin-4.0.2-i3` 区分；签发仍须三方一致 + 用户批准 |
| SOURCE_DATE_EPOCH | **沿用 `1788796800`**（= 2026-09-07 16:00 UTC，4.0.2-i3 已审核值） | ① epoch 仅决定字节确定性，与版本比较无关（升级序由版本串保证）；② 沿用已审核值零新增审核面；③ 与 4.0.2-i3 同 epoch 使双血统产物比对时**排除 mtime/epoch 造成的时间差异**（版本/包名/构建配置/工具链/载荷本身的差异仍会体现在字节中，epoch 仅排除时间戳一类） |
| 替代方案（不推荐） | 新固定值 `1789056000`（= 2026-09-10 16:00 UTC） | 若项目惯例要求 release 间区分可用此值；但签发日尚未定，取"未来日期"反直觉且需新增审核。**提交 dsh/用户批准后定稿** |
| 包内 mtime | 全部文件 `touch -h -d @1788796800`（同 4.0.2-i3 规则） | 确定性；与 SOURCE_DATE_EPOCH 一致 |

## 三、builder 集成设计（设计稿，不动 builder）

1. **DKMS 源改 O_stage**：5.0.0-i1 分支的 DKMS 源根 = materialize-o-stage.sh 产物解包（替代 4.0.2-i3 流程中 D_stage 源 + 内联 patch 的路径）；builder 脚本增加 `5.0.0-i1` 分支（EXPECTED_SOURCE_DATE_EPOCH=1788796800、源快照引用 o-stage-snapshot.tar.zst、13 条 030-NNN 不再由 builder 内联应用而是验证 materialize 产物）；
2. **vermagic**：模块 vermagic 由内核构建体系自动生成，运行时矩阵记录（沿用 4.0.2-i3 验证口径）；
3. **载荷边界**：debs/ 产物 = fantgpu 血统 5.0.0-i1 包（fantgpu-fh2m-kernel）；包内文件清单与 4.0.2-i3 对齐（fant* 命名树）；
4. **patch-000 no-transform 声明**：builder 无任何 o_shipped 字节变换步骤（F0 对象原样，关闭项决策批已裁定 no-transform，PLL 语义风险 UNVERIFIED 登记）；
5. **不安装契约**：builder 仅构建；安装/回退由验证矩阵阶段手动执行（同 4.0.2-i3 先例）；
6. **双 clean-build 字节一致证据计划**：5.0.0-i1 双 clean-build（build-A/B）SHA-256 比对，与 4.0.2-i3 证据同构。

## 四、验证计划（阶段三验证矩阵清单）

| 维度 | 验证项 | fixture / 工具 |
| --- | --- | --- |
| 静态 | materialize-o-stage.sh 单测（路径边界 / fail-closed / 链点校验 / 幂等 / 事务恢复） | `tests/unit/run-o-stage-materialize-tests.sh`（**已交付**，18 用例：路径越界/symlink 拒绝/SHA 不符/坏输入/工具版本锁/事务故障注入（commit 与 staged_done）/恢复/幂等/链点 fail-closed/回滚失败 rolling_back 保留现场/人工裁决后恢复/committed 写入失败自洽恢复/未知与损坏 journal fail-closed/恢复清理失败 fail-closed；与 O-4 测试同构） |
| 静态 | 13 条 030-NNN meta 校验 + schema + 文档 + 许可 | `tests/unit/run-030-meta-tests.sh`（**已交付**：逐条校验 030-*.meta.json schema 1.0 字段 + apply 链点哈希衔接 + patch SHA 一致）+ 既有 `check-docs.sh` + `validate-collab.py` + `r16-gate.py` + `tools/audit-licenses.py` |
| 静态 | O_stage 编译通过 | gcc/clang 编译门禁（DKMS 构建即覆盖） |
| 运行时 | DRM device open / fbdev mmap / DMA-BUF self-import / VA-API 解码 | run-capability-baseline.sh + run-dmabuf-regression-test.sh + run-vaapi-decode-test.sh |
| 运行时 | suspend/resume（024 + 026-lifecycle + 028 合并覆盖） | probe-suspend-resume-state.sh |
| 运行时 | inactive CRTC vblank 快速 EINVAL（026） | probe-drm-vblank.c |
| 运行时 | DDCCI panel 显式逻辑（029）+ 2880x1800 刷新率（006）+ 2560 base-vs-base 观察（006 登记） | probe-drm-topology.c + 实机面板 |
| 运行时 | **UNVERIFIED 登记 1**：025-display 唤醒显示观察（i4-only 假设不迁移，观察 F0 维持 i3 语义的行为） | probe-suspend-resume-state.sh 扩展观察项 |
| 运行时 | **UNVERIFIED 登记 2**：patch-000 G0M GPU PLL 双重初始化症状观察（no-transform 操作结论，语义未闭合） | 阶段三矩阵观察项 |
| 安装/回退 | 5.0.0-i1 dpkg 安装 + 完整系统快照回退往返 | 手动记录 + 验证脚本（同 4.0.2-i3 install-rollback 先例） |
| 门禁汇总 | check-docs rc=0 + validate-collab rc=0 + r16-gate rc=0 | 每批提交后实跑并记录 |

## 五、产物落点

- 目录性质：`docs/planning/evidence/o-stage/` 是**阶段三的证据与产物写入目录**（阶段三规范明确）；其中**已有 F0 O-4 输入只读**（`f0-snapshot.tar.zst`/`.sha256`/`f0.manifest.tsv`/`o4-f0.genesis.json`，O-4 锁定产物不得改动），**新生成的 o-stage-* 产物按 §一事务契约写入**（与「保护区零写入」的保护区定义不冲突——保护区指 debs/vendor/build/third_party 及 drivers/baselines 等既定范围，本目录为阶段三指定写入点）
- 产物清单：
  - **materialize 事务产物（五件同代提交、禁止混代、meta 不可变）**：
    - `docs/planning/evidence/o-stage/o-stage-snapshot.tar.zst`
    - `docs/planning/evidence/o-stage/o-stage-snapshot.tar.zst.sha256`
    - `docs/planning/evidence/o-stage/o-stage.manifest.tsv`
    - `docs/planning/evidence/o-stage/o-stage.manifest.tsv.sha256`
    - `docs/planning/evidence/o-stage/5.0.0-i1.meta.json`：不可变 provenance（版本串 / SOURCE_DATE_EPOCH / tag 名 / O_stage 树 hash / 13 条 030-NNN 链 / 两条 UNVERIFIED 登记 / 快照四文件各自 SHA-256；materialize 时一次写完，之后不改）
  - **验证结论文件（独立于同代产物集）**：
    - `docs/planning/evidence/o-stage/5.0.0-i1-validation-results.json`：阶段三矩阵逐项结论，验证阶段写入，签发前填充全 PASS 状态；**对 immutable meta 的单向绑定**（记录 materialize_meta_sha256，验证阶段校验一致后方可填充；自身完整性由最终 Git commit / annotated tag 锚定）

## 六、签发链

O_stage 构建集成 → 阶段三验证矩阵全 PASS（= `5.0.0-i1-validation-results.json` 全 PASS 状态）→ 三方一致 + dsh 终审 + 用户批准 → 签发 annotated tag `fantgpu-5.0.0-i1`（仅此名）。
