# O_stage 构建集成方案（5.0.0-i5 F 诊断分支）——R5 停批修订

- 日期：2026-09-16
- 状态：**i4 启动 Oops 已定位，030-033/i5 ABI 修正、双构建与首次启动健康验证完成，待 qoder 正式初审/dsh 终审；R5 仍为 FAIL**（17 链树 hash `4be9ba75…`，候选 SHA `6e491677…`；debugfs 探针未执行；不创建 validation-results、不签发、不打 tag）
- 放行依据：dsh 放行规格（已记 report.md）；约束不变（保护区零写入、debs/F0 只读、builder 不安装不重启）
- 前置事实：i2、i3 与 i4 均为失败档案锚点，保持不覆盖。i4 安装后首次启动 `6.12.101+deb13-amd64` 在 `fixup_pcie_init` Oops；根因是 030-032 把 probe 状态插入闭源 `fantgpu.o_shipped` 共享的 `struct dev_rsrc`，破坏固定字段偏移。030-033 恢复 i3 结构布局并把状态改为独立 devres，形成 17 链树 `4be9ba7509258726b95bd41137da1280868c91c835aa57d4150fd6ba76913300`。

## 一、materialize-o-stage.sh 设计（编排物化）

**目标**：从 O-4 只读输入可复现地物化 O_stage 源树快照，作为 5.0.0-i5 F builder 的 DKMS 源根，同时保持 i2/i3/i4 五件失败档案字节不变。

**流程**（脚本化，每步校验、fail-closed）：

1. 前置校验：tar 1.35 / zstd 1.5.7（工具版本锁定，同 O-4 先例）；输入 `docs/planning/evidence/o-stage/f0-snapshot.tar.zst` SHA == `65fa17ec…`；17 条 `patches/030-*.patch` 的 SHA 与各自 meta.json 的 `apply.patch_sha256` 一致；
2. 解包 F0 快照至事务工作目录（排他锁 + .txn 目录模式，同 O-4 事务先例），校验解包树 hash == `7219d817…`（O-4 锁定值）；
3. 按链序应用 17 条 030-NNN（`patch --batch --forward --fuzz=0 --no-backup-if-mismatch -p1`）：
   001 → 002 → 006 → 009 → 007 → 023 → 025 → 024 → 026 → 027 → 026-lifecycle → 028 → 029 → 030 → 031 → 032 → 033；
   每条应用后校验树 hash == 该条 meta.json 的 `apply.after_tree_hash`（逐链点校验，链基 = 前一条 after）；
4. 最终校验：树 hash == `4be9ba75…`；
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
   - `5.0.0-i5.meta.json`：**当前代 provenance**（版本串/tag、SOURCE_DATE_EPOCH、O_stage 树 hash `4be9ba75…`、17 条链及四文件 SHA）；五件写入独立目录 `docs/planning/evidence/o-stage/5.0.0-i5/`。i2/i3/i4 五件不被该事务覆盖。

**meta.json 生命周期（消除同代冲突）**：

- materialize 阶段：`5.0.0-i5/5.0.0-i5.meta.json` 作为 provenance 在事务内生成，全部字段在 materialize 时已知（含快照四文件 SHA），随五件同代产物统一提交；此后不可变（任何字段变更 = 新的受控事务 + 全套产物重生成/重校验）；
- 验证阶段：`docs/planning/evidence/o-stage/5.0.0-i5-validation-results.json` 仍冻结且不存在；只有受监督定位、真实 suspend/resume 和恢复门全部闭合后才能创建并填充；
- 两者绑定关系（**单向绑定，validation 锚定 immutable meta**）：
  - meta.json 固定记录 validation-results 文件路径与 schema（meta 只引用文件名，不引用其哈希——validation 后生成，其哈希在 materialize 时不可知）；
  - validation-results 记录 `materialize_meta_sha256`（= i5 meta 的 SHA-256，验证阶段校验一致后方可填充结论）；
  - validation-results 自身完整性由最终 Git commit / annotated tag（fantgpu-5.0.0-i5）或独立 validation sidecar 锚定——不做互引 SHA-256。

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
- 所有 i5 产物（**五件完整：tar + 快照 sidecar + manifest + manifest sidecar + 5.0.0-i5.meta.json**）先写入同一事务目录并完成内部校验；i2、i3、i4 与 i5 由目录隔离；
- 提交点：全部校验通过后按序原子替换到最终位置；journal 记录状态机（staged/committed/rolling_back），每步 fsync（文件 + 目录）；
- 失败/中断：原有完整产物保持不变；恢复入口按 journal 状态幂等续跑（O-4 `recover_interrupted` 口径）；幂等重跑规则 = 相同输入重跑产出字节一致的产物；
- **禁止混代**：任何状态下不允许新 tar + 旧 manifest（或反向）共存于最终位置——产物对（tar+sha256+manifest+sha256+meta）要么全部同代、要么全部保持旧代。

**快照参数先例**：tar 1.35 / zstd 1.5.7 / @1640995200 / --sort=name / owner·group 0 / transform 前缀 `o-stage`——与 O-4（前缀 f0）与 D_stage 先例完全同构，仅前缀不同。

## 二、版本与 epoch 定稿（2026-09-16 阶段 D：新候选 i5；i2/i3/i4 保持失败锚点）

| 字段 | 定稿值 | 理由 |
| --- | --- | --- |
| 版本串 | `5.0.0-i5` | 030-033 修正 i4 启动 ABI 回归，必须新代；i2/i3/i4 不复用、不覆盖 |
| tag 名 | `fantgpu-5.0.0-i5`（仅前向保留，当前禁止创建） | 签发仍须 R5 与完整验证闭合、三方一致及用户批准 |
| SOURCE_DATE_EPOCH | **`1789516800`**（2026-09-16 00:00 UTC） | i5 固定审核 epoch；升级序仍由版本串保证 |
| 包内 mtime | 全部文件 `touch -h -d @1789516800` | 确定性；与 SOURCE_DATE_EPOCH 一致 |

## 三、builder 集成（5.0.0-i5 F 分支静态完成；真机验证冻结）

1. **DKMS 源 O_stage**：i5 分支只消费 `5.0.0-i5/` 五件，校验快照 SHA、i5 meta 与树 hash `4be9ba75…`；17 链已在 materialize 阶段应用，builder 不做 post-trace 补丁；
2. **vermagic**：双构建均校验 `fantgpu.ko` 的 vermagic 前缀匹配当前内核；
3. **载荷边界**：包名、DKMS 名、模块名与 F userspace/固件边界不变；包内源树为 17 链 i5 快照，含 030-030、030-031、030-032 与 030-033；闭源对象保持 no-transform；
4. **归档代防串代**：builder 明确拒绝用当前 i5 快照构建 5.0.0-i1/i2/i3/i4；
5. **不安装契约**（已实现）：builder 仅构建；安装/回退由验证矩阵阶段手动执行；
6. **保护区边界**：STAGE_ROOT/BUILD_LOG/OUT_DEB 可注入；未来 i5 真机原始证据只落 ignored `.runtime-archive/runtime-5.0.0-i5/`，Git 仅保留脱敏摘要；不创建 validation-results、不打 tag；
7. **定案项（如实记录）**：check-release-package.sh 的静态门禁已闭合；C2 options 文件由包内确定性 payload 承载。上述静态门禁不等于 F 显示运行时闭合；2026-09-14 中止记录已证明必须增加 runtime health gate。
8. **双 clean-build 字节一致证据**：`build-5.0.0-i5.sha256` 记录 A/B deb SHA `6e491677…` 与 562 行 md5sums 一致；两次均显式针对 `6.12.101+deb13-amd64`，且编译后 BTF ABI 门通过；这只证明构建可复现，不改变 `R5=FAIL`。

## 四、验证计划（阶段三验证矩阵清单）

| 维度 | 验证项 | fixture / 工具 |
| --- | --- | --- |
| 静态 | materialize-o-stage.sh 单测（路径边界 / fail-closed / 链点校验 / 幂等 / 事务恢复） | `tests/unit/run-o-stage-materialize-tests.sh`（19 用例全过；与 O-4 测试同构） |
| 静态 | 17 条 meta + marker/probe 契约 + shipped-object ABI | `run-030-meta-tests.sh`、`run-030-031-pm-marker-tests.sh`、`run-030-032-pm-probe-tests.sh`、`run-030-033-shipped-abi-tests.sh`；builder 对真实 `.ko` 执行 `pahole` 大小/偏移门 |
| 静态 | O_stage 编译通过 | gcc/clang 编译门禁（DKMS 构建即覆盖） |
| 运行时 | DRM device open / fbdev mmap / DMA-BUF self-import / VA-API 解码 | run-capability-baseline.sh + run-dmabuf-regression-test.sh + run-vaapi-decode-test.sh |
| 运行时前置 | F 显示健康门禁：特权 dmesg、固件请求、DRM card 注册、kernel fault | `scripts/check-fantgpu-runtime-health.sh`；合成 fixture 只验证 fail-closed 控制流，不证明预编译 HAL bind 或真实硬件可用 |
| 运行时 | suspend/resume（024 + 026-lifecycle + 028 合并覆盖） | probe-suspend-resume-state.sh |
| 运行时 | inactive CRTC vblank 快速 EINVAL（026） | probe-drm-vblank.c |
| 运行时 | DDCCI panel 显式逻辑（029）+ 2880x1800 刷新率（006）+ 2560 base-vs-base 观察（006 登记） | probe-drm-topology.c + 实机面板 |
| 运行时 | **UNVERIFIED 登记 1**：025-display 唤醒显示观察（i5-only 假设不迁移，观察 F0 维持 i3 语义的行为） | probe-suspend-resume-state.sh 扩展观察项 |
| 运行时 | **UNVERIFIED 登记 2**：patch-000 G0M GPU PLL 双重初始化症状观察（no-transform 操作结论，语义未闭合） | 阶段三矩阵观察项 |
| 安装/回退 | 5.0.0-i5 已受监督安装；i4 保留失败锚点 | i5 已在 `6.12.101` 首次启动并通过模块、DRM、硬件 GL 与 DRI3 健康门；探针未执行。安装脚本只构建当前运行内核，导致从 `6.12.107` 安装时失败；本地 target override 仅供诊断恢复，发布前必须修复 postinst |
| 门禁汇总 | check-docs rc=0 + validate-collab rc=0 + r16-gate rc=0 | 每批提交后实跑并记录 |

## 五、产物落点

- 目录性质：顶层 F0 与 i2/i3/i4 五件只读；i5 五件只写 `docs/planning/evidence/o-stage/5.0.0-i5/`，以目录隔离防止覆盖失败档案。
- 产物清单：
  - **materialize 事务产物（五件同代提交、禁止混代、meta 不可变）**：
    - `docs/planning/evidence/o-stage/5.0.0-i5/o-stage-snapshot.tar.zst` + `.sha256`
    - `docs/planning/evidence/o-stage/5.0.0-i5/o-stage.manifest.tsv` + `.sha256`
    - `docs/planning/evidence/o-stage/5.0.0-i5/5.0.0-i5.meta.json`
  - **验证结论文件（独立于同代产物集）**：
    - `docs/planning/evidence/o-stage/5.0.0-i5-validation-results.json`：当前不得创建；获批验证且全项闭合后才允许写入，并单向绑定 i5 meta。

## 六、签发链

O_stage 构建集成 → i5 在 `6.12.101` 正常启动与健康门 → 重新审批的诊断序列 → R5 真实 suspend/resume 与阶段三验证矩阵全 PASS → 三方一致 + dsh 终审 + 用户批准 → 才可签发 annotated tag `fantgpu-5.0.0-i5`。当前链停在 i5 正式批初审前，运行时仍冻结。
