# D → D_stage 完整性审计（O-2 v24 · tree-manifest 可执行 + 9 类 symlink 闭合 + F-only 移除 + atomic commit + 跨树判定显式定义 + `realpath -m` canonicalize + 三子命令接口 + 目录级 staging + reconcile-first + tar.zst 精确版本锁 + 排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal 协议（含旧对精确指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 旧对完整性 fail-closed + 幂等 no-op（复用严格 sidecar 校验）+ selfcheck fail-closed + fsync 状态依赖数据屏障（断电一致性；跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行等价判定）+ 无 journal 恢复走全新路径 + Python 实现契约 + 启动恢复 + 故障注入测试 28 场景（含 power-loss）+ `--reference-manifest` CLI 解析 + meta.json 路径统一 + jq 工具链前置 + 退出码子命令作用域契约）

> 创建日期：2026-09-05（v5: 2026-09-05 dsh 三阶段返工指令后；v6: 2026-09-05 codex v5 初审 P1 #3 + P2 #5 闭环后；v7: 2026-09-05 codex v6 初审 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 闭环后；v8: 2026-09-05 codex v7 初审 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P1 #6 + P1 #7 + P2 #8 闭环后；v9: 2026-09-05 codex v8 初审 6 P1 + 2 P2 闭环后；v10: 2026-09-05 codex v9 初审 5 P1 + 3 P2 闭环后；v11: 2026-09-05 codex v10 初审 4 P1 + 1 P2 闭环后；v12: 2026-09-05 codex v11 初审 2 P1 + 2 P2 闭环后；v13: 2026-09-05 codex v12 初审 4 P1 + 1 P2 闭环后；v14: 2026-09-05 codex v13 初审 2 P1 + 2 P2 闭环后；v15: 2026-09-05 codex v14 初审 2 P1 + 2 P2 闭环后；v16: 2026-09-05 codex v15 初审 2 P1 + 1 P2 闭环后；v17: 2026-09-05 codex v16 初审 2 P1 + 1 P2 闭环后；v18: 2026-09-05 codex v17 初审 2 P1 + 2 P2 闭环后；v19: 2026-09-06 codex v18 初审 2 P1 + 1 P2 闭环后；v20: 2026-09-06 codex v19 初审 1 P1 + 1 P2 闭环后；v21: 2026-09-06 codex v20 初审 3 P1 闭环后；v22: 2026-09-06 codex v21 初审 1 P1 闭环后；**v23: 2026-09-06 codex v22 初审 1 P1 闭环后**；v24: 2026-09-06 codex v23 初审 1 P1 + 1 P2 闭环后）
> 起草：qoder
> 状态：**v24 已修复 v23 一项问题（per codex v23 P1 #1）：回滚 fsync 失败退出码契约冲突——两处伪代码将逐次还原后的 fsync_dir 失败定义为 exit 9，与 §五 Python 契约"所有 fsync_file/fsync_dir 失败均 exit 5"冲突，且错误码表 snapshot 5 未覆盖回滚段 fsync 失败；v24 统一为 exit 5（journal 保持 rolling_back，下次启动重入收敛），伪代码 / 错误码表（snapshot 5 扩展）/ 退出码测试说明同步；并新增 power_loss_rollback_restore_fsync_window 故障场景（回滚还原 mv 后、任一目录 fsync 未完成时断电 → 三态契约：一致事实可重入 / 旧文件双存在 / 均缺失 → 三者事实校验 exit 9 保留证据，fail-closed；per codex v23 P2 #2）；故障注入 29 场景（+1）+ genesis schema 2.4 / fault_injection_tests 13.0**
> 上游：`fantgpu-base-update-evaluation.md`（P5 评估产出）
> 下游：`docs/planning/030-patch-rederivation-design.md` §四.5 / §八 / §〇.1
> 配套：`docs/planning/030-mapping-table.md`（O-1 v12）
> **本文件为阶段一 O-2 方法论定稿 + 生成脚本占位 + 工具链规范 + tree-manifest
> 锁定规范；实际生成工作由 qoder 在 D_stage 就位后执行，输出路径为非保护区
> `docs/planning/evidence/` 下 **9 文件**（**v10 per codex v7 P1 #6** = 8 稳定
> 证据 + 1 运行时 genesis.json，详 §十一 v24）**；D_stage 物化根 =
> `/tmp/r16-d-stage/`**（v24 唯一合法路径，系统 `$TMPDIR`，不在仓库任何路径
> 下，per §〇.5 + §四.1 + §五.3 v24）**。

## 一、v24 重定义摘要（per codex v7 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P1 #6 + P1 #7 + P2 #8 + codex v8 6 P1 + 2 P2 + codex v9 5 P1 + 3 P2 + codex v10 4 P1 + 1 P2 + codex v11 2 P1 + 2 P2 + codex v12 4 P1 + 1 P2 + codex v13 2 P1 + 2 P2 + codex v14 2 P1 + 2 P2 + codex v15 2 P1 + 1 P2 + codex v16 2 P1 + 1 P2 + codex v17 2 P1 + 2 P2 + codex v18 2 P1 + 1 P2 + codex v19 1 P1 + 1 P2 + codex v20 3 P1 + codex v21 1 P1 + codex v22 1 P1 + codex v23 1 P1 + 1 P2 闭环）

| 维度 | v7 | v8 |
| --- | --- | --- |
| **O-2 范围** | D → D_stage 完整性审计（**非** D→F0） | 同 v7（**非** D→F0 宽 diff，已降级为阶段三辅助） |
| **输入锁定** | tree-manifest 锁定：含文件类型 + 相对路径 + symlink target + 文件 SHA-256（详 §三） | 同 v7（v8 强化 §三 / §十一 字段编码规则，详 §三 v8 + §十一 v8） |
| **tree-manifest 命令** | **v7 类型分支 + NUL 安全 + 显式 `SRC_ROOT` 命令行参数**（详 §四.3）：`find -type d` / `find -type l` / `find -type f` 三次独立枚举 + `-print0` + `xargs -0` + `sort -z` + 临时文件 + 原子 rename + 旧产物保护 + 工具版本校验 + 字段数校验 | **v8 修正 v7 残留问题**（per codex v7 P1 #2 + P1 #3）：SRC_ROOT 通过 `bash -c` 子进程参数显式传入（而非依赖 shell 变量）；最终 TSV 写入前**逐行校验**不含 tab/CR/LF/NUL（filename / symlink target）；readlink 失败立即 exit 5 而非 echo '' 吞错；详 §四.3 v8 |
| **D_stage 物化路径** | `/tmp/r16-d-stage/`（v7 唯一合法路径） | 同 v7 |
| **tree-manifest 排序** | LC_ALL=C 字节序 + (文件类型, 路径) 二级排序 + `find -print0` + `xargs -0` 字节安全 | 同 v7（v8 强化：sort -t$'\t' -k1,1 -k2,2 + 二次 awk NF=4 校验） |
| **symlink 分类** | **9 类**（v7 per codex v6 P1 #4：same-target / target-changed / single-side-d / single-side-dstage / d2dstage / dstage2d / dangling / broken / loop） | 同 v7 |
| **symlink BC 归属** | SYM-N-A 不归入 BC | 同 v7 |
| **主表 vs 专表对齐** | 9 文件输出含 2 输入 lock | 同 v7（v8 §九 schema 校验路径修复：分别校验 D.manifest.tsv 与 D_stage.manifest.tsv，per codex v7 P1 #4） |
| **F0 引用检测** | 退出码 4（F0 reference violation） | 同 v7 |
| **F-only 文件归属** | v7 通用规则：任何仅 F0 存在的文件出现在 D / D_stage 中即视为违反（**不预设**文件名清单；F-only 文件名清单仅在阶段三裁决阶段 O-4 闭合后由 dsh 提供） | **v8 删除**：O-2 **不进行** F-only 检测（per codex v7 P1 #1：不读取 F0 + 不持有 F-only 文件清单的情况下无法计算 F-only 性质）；**F-only 检测整体移到阶段三 O-4 闭合后**（阶段三必须先读 F0 才可能定义 F-only）；O-2 §十三 / §十四 / §十五 F-only 相关判据全部删除；§九 退出码 6 删除；§十一 genesis.json `f_only_exclusion_check` 字段删除 |
| **§十三 O-2 闭合判据** | v7 仅做 D → D_stage 完整性闭合（**不引用** P5 任何 per-file 列表 / 23 BC 矩阵 / 435 per-file 证据） | **v8 删除 F-only 通用规则项**（per codex v7 P1 #1）；F-only 决策全部移到阶段三 O-4 后 |
| **§十一 hash 规范** | 9 文件 + 各自 sha256（v7 per codex v6 P1 #5） | **v8 拆分稳定性证据 vs 运行时元数据**（per codex v7 P1 #6）：稳定 8 文件（4 输入 lock + 4 产物 tsv）+ 各自 sha256（构成"9 文件 SHA 一致"判定基础） + 1 个 runtime/genesis.json（**不计入** SHA 一致判定，仅写入文件本身）；v12 沿用 v8 拆分（详 §十一 v12）；**v11 per codex v10 P1 #3 新增 genesis.json `tool_versions.jq` + `fault_injection_tests` 字段；v12 per codex v11 P1 #1 扩展 `fault_injection_tests` 至 7 场景 + per codex v11 P2 #4 新增 `exit_code_test_coverage` 字段** |
| **§五 tar.zst 规范** | v7 仅提及 `d-stage-snapshot.tar.zst` 嵌入 release commit（命令未定义） | **v8 完整 tar.zst 可复现规范**（per codex v7 P1 #7）：固定 `--sort=name --mtime=@EPOCH --owner=0 --group=0 --numeric-owner --no-acls --no-xattrs --no-selinux --transform='s,^...,...,'` + 显式 zstd 版本/level + tarball 与 manifest reconcile 命令；详 §五.4 v8 |
| **§五.3 Git tag 身份** | v7 候选命名 `v4.0.2-i3` / `r16-4.0.2-i3`（待 dsh 确认） | **v8 显式待 dsh 确认**（per codex v7 P2 #8）：tag 身份必须在阶段二启动前由 dsh 反查 `git tag -l` 后给唯一名称（区分包版本 `4.0.2-i3` 与 Git ref）；详 §五.3 v8 |

## 二、D → D_stage 完整性审计的定义

> 验证阶段一确实收齐了所有当前 Deepin 修改：
>
> - D 源树 + 阶段一中性变更记录声明的有效 patch 清单 + 应用顺序
>   → 应用 → D_stage 源树
> - D_stage 源树 - D 源树 = 阶段一中性变更记录声明的所有修改集
>   （无 hidden change / 无 missing patch）
>
> 即：D_stage 是 D + 中性记录的可重放结果。

## 三、生成工具链总体流程

```
[D 源树根]              [D_stage 源树根]
   (tree-manifest 锁定)    (tree-manifest 锁定)
       │                          │
       └──────────┬───────────────┘
                  ▼
   Step 1：tree-manifest 枚举（find -mindepth 0 + LC_ALL=C sort）
                  │  4 字段：file_type / relative_path / symlink_target / file_sha256
                  ▼
   Step 2：分类（differs / identical / D-only / D_stage-only / symlink-* 9 类）
                  │
                  ▼
   Step 3：sort 归一化（LC_ALL=C, (file_type, path) 二级排序）
                  │
                  ▼
   Step 4：symlink 处置（不 follow；9 类细分；SYM-N-A 不归入 BC）
                  │
                  ▼
   Step 5：主表 6 字段 + 专表 7 字段 schema 写入（atomic write + fsync）
                  │
                  ▼
   Step 6：输出 SHA-256 计算 + 写入 .sha256 文件（八件套 + 各自 sha256；genesis.json 不计 SHA 一致判定，仅写入文件本身，详 §十一 v8）
                  │
                  ▼
   Step 7：确定性自校验（同一输入两次运行 SHA-256 一致；不含 F-only 检测，per codex v7 P1 #1）
                  │
                  ▼
   八件套 + 各自 sha256 + 1 个 runtime/genesis.json：
   - docs/planning/evidence/d-stage-audit.tsv (+ .sha256)
   - docs/planning/evidence/d-stage-audit.symlink.tsv (+ .sha256)
   - docs/planning/evidence/d-stage-audit.genesis.json (运行时元数据，不计 SHA 一致判定)
   - docs/planning/evidence/d-stage-audit.D.manifest.tsv (+ .sha256)
   - docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv (+ .sha256)
```

## 四、输入锁定：tree-manifest 规范（v7 替代 v6 find-exec readlink 命令）

> **v6 错误纠正**：`sha256sum <D 源树根>` 对目录会失败（`sha256sum` 仅计算
  文件内容哈希，不生成目录树摘要）；v5 §四 / §十一 写法已**删除**；v6
  改用 tree-manifest 锁定，但 `find -exec readlink` 命令对非 symlink
  类型会产生额外输出且 `SRC_ROOT` 未定义；v7 进一步改用**类型分支 + NUL
  安全枚举 + 显式 `SRC_ROOT` 参数 + 每步 rc + 临时文件 + 原子替换 + 旧
  产物保护**，命令可直接重放。

### 4.1 tree-manifest 字段规范（每行 4 字段，固定）

```
<file_type> \t <relative_path> \t <symlink_target> \t <file_sha256>
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `file_type` | enum | `f`（regular file）/ `d`（directory）/ `l`（symlink） |
| `relative_path` | path | 相对源树根的路径；统一 `/` 分隔符（不混用 `\`）；去除 `./` 前缀；末尾不带 `/` |
| `symlink_target` | path / 空 | symlink 类型时必填 symlink target 路径；`f` / `d` 时填空字符串 |
| `file_sha256` | sha256 | `f` 类型时必填 SHA-256（文件内容哈希）；`d` 时填空字符串；`l` 时填空字符串（symlink 本身无内容哈希） |

### 4.2 tree-manifest 排序规范（确定性）

- **排序键**：(file_type, relative_path) 二级 LC_ALL=C 字节序排序；
- **file_type 字典序**：`d` < `f` < `l`（C locale 下 ASCII 顺序：100 < 102 < 108）；
- **不变量**：相对路径按字节序排序（不区分大小写 / 不 locale-aware）。

### 4.3 tree-manifest 生成命令（v8 per codex v7 P1 #2 + P1 #3 + P1 #4 + P1 #5 修订；可直接重放）

```bash
#!/usr/bin/env bash
# v8 tree-manifest generator（可直接重放的脚本片段）
# 输入：D_SRC_ROOT / DSTAGE_SRC_ROOT（**显式**绝对路径；不依赖外部环境变量）
# 输出：<out-dir>/d-stage-audit.{D,D_stage}.manifest.tsv (+ .sha256)
# 硬约束：LC_ALL=C / findutils ≥ 4.9 / sort 字节序 / NUL 安全 / 原子写
# v8 修正（per codex v7 P1 #2 + P1 #3 + P1 #4 + P1 #5）：
#   - SRC_ROOT 通过 env 显式 export 给 xargs bash -c 子进程（per P1 #2：
#     v7 sh -c 子进程不继承调用方 shell 变量导致 sha256sum 失败 exit 123）
#   - 最终 TSV 写入前逐行校验字段不含 tab/CR/LF/NUL（per P1 #3：filename /
#     symlink_target 含控制字符会破坏 TSV 字段边界）
#   - readlink 失败 → exit 5（input read error），不再 `|| echo ''` 吞错
#     （per P1 #3）
#   - validate-then-replace：全部校验通过后才 mv 到最终路径（per P1 #5：
#     v7 旧产物保护在校验前 backup 路径冲突）

set -euo pipefail
LC_ALL=C
export LC_ALL

D_SRC_ROOT="${1:?missing D 源树根绝对路径}"
DSTAGE_SRC_ROOT="${2:?missing D_stage 源树根绝对路径}"
OUT_DIR="${3:?missing 输出目录绝对路径}"
LABEL="${4:?missing label (D or D_stage)}"   # "D" 或 "D_stage"

case "$LABEL" in
  D|D_stage) ;;
  *) echo "ERROR: LABEL 必须是 D 或 D_stage" >&2; exit 78 ;;
esac

# 工具版本校验（任何不符合 → exit 78 EX_CONFIG）
FIND_VER=$(find --version 2>/dev/null | head -1 | awk '{print $NF}')
SORT_VER=$(sort --version 2>/dev/null | head -1 | awk '{print $NF}')
SHA256_VER=$(sha256sum --version 2>/dev/null | head -1 | awk '{print $NF}')
# findutils ≥ 4.9：'4.9.0' ≥ 4.9 → 通过
case "$FIND_VER" in
  4.9*|4.1[0-9]*|5.*|6.*|7.*|8.*|9.*) ;;
  *) echo "ERROR: findutils ≥ 4.9 required (got $FIND_VER)" >&2; exit 78 ;;
esac

# 选取 SRC_ROOT
case "$LABEL" in
  D)        SRC_ROOT="$D_SRC_ROOT" ;;
  D_stage)  SRC_ROOT="$DSTAGE_SRC_ROOT" ;;
esac

# v8 P1 #2 修正：SRC_ROOT 通过 env 显式 export 给后续 xargs bash -c 子进程
# （v7 `sh -c` 子进程不继承调用方 shell 变量，sha256sum 路径失败 exit 123）
export SRC_ROOT

# 临时文件（validate-then-replace：所有校验通过后才原子 rename 到最终路径）
TMP_MANIFEST="${OUT_DIR}/.d-stage-audit.${LABEL}.manifest.tsv.tmp.$$"
TMP_SHA="${OUT_DIR}/.d-stage-audit.${LABEL}.manifest.tsv.sha256.tmp.$$"
TMP_D="${TMP_MANIFEST}.d"
TMP_L="${TMP_MANIFEST}.l"
TMP_F="${TMP_MANIFEST}.f"
trap 'rm -f -- "$TMP_MANIFEST" "$TMP_SHA" "$TMP_D" "$TMP_L" "$TMP_F"' EXIT

# === Step 1：枚举 directory / symlink（不含 regular file） ===
# 类型分支：分别处理 d / l，避免 find -exec readlink 对 f 类型产生额外输出
# NUL 安全：使用 -print0 + xargs -0 字节安全
# 排序：LC_ALL=C 字节序，键 = (file_type, relative_path)

# 目录条目
LC_ALL=C find "$SRC_ROOT" -mindepth 0 -type d -printf 'd\t%P\0' \
  | LC_ALL=C sort -z -t$'\t' -k1,1 -k2,2 \
  > "$TMP_D"

# symlink 条目（v8 P1 #3 修正：readlink 失败 → exit 5，不再 || echo '' 吞错）
LC_ALL=C find "$SRC_ROOT" -mindepth 0 -type l -printf 'l\t%P\0' \
  | LC_ALL=C sort -z -t$'\t' -k1,1 -k2,2 \
  | while IFS=$'\t' read -r -d $'\0' ftype fpath; do
      if ! target=$(LC_ALL=C readlink "${SRC_ROOT}/${fpath}" 2>/dev/null); then
        echo "ERROR: readlink failed for ${SRC_ROOT}/${fpath}" >&2
        exit 5
      fi
      printf '%s\t%s\t%s\t\n' "$ftype" "$fpath" "$target"
    done \
  > "$TMP_L" || { rm -f -- "$TMP_D" "$TMP_L" "$TMP_F"; exit 5; }

# === Step 2：枚举 regular file 并计算 SHA-256 ===
# v8 P1 #2 修正：SRC_ROOT 通过 env 显式传给 xargs bash -c 子进程
# 子进程不再依赖调用方 shell 变量（v7 sh -c 子进程因不继承调用方
# 局部变量，sha256sum 路径失败 → exit 123）
LC_ALL=C find "$SRC_ROOT" -mindepth 0 -type f -printf '%P\0' \
  | LC_ALL=C sort -z \
  | LC_ALL=C xargs -0 -I {} env SRC_ROOT="$SRC_ROOT" bash -c '
      fpath="$1"
      if [ -z "$SRC_ROOT" ]; then
        echo "ERROR: SRC_ROOT not exported into xargs bash -c subprocess" >&2
        exit 78
      fi
      sha=$(LC_ALL=C sha256sum "${SRC_ROOT}/${fpath}" 2>/dev/null | LC_ALL=C awk "{print \$1}")
      if [ -z "$sha" ]; then
        echo "ERROR: sha256sum failed for ${SRC_ROOT}/${fpath}" >&2
        exit 5
      fi
      printf "f\t%s\t\t%s\n" "$fpath" "$sha"
    ' _ {} \
  > "$TMP_F" || { rm -f -- "$TMP_D" "$TMP_L" "$TMP_F" "$TMP_MANIFEST" "$TMP_SHA"; exit 5; }

# === Step 3：合并三类条目 + 排序 + 字段编码校验（v8 P1 #3 增强） ===
# 合并后先逐行校验 filename / symlink_target / file_sha256 字段不含
# tab/CR/LF/NUL 等控制字符（避免破坏 TSV 字段边界）
cat -- "$TMP_D" "$TMP_L" "$TMP_F" \
  | LC_ALL=C sort -t$'\t' -k1,1 -k2,2 \
  | LC_ALL=C awk -F'\t' '{
      if (NF != 4) { print "ERROR: NF=" NF " at line " NR ": " $0 > "/dev/stderr"; exit 1 }
      for (i = 2; i <= 4; i++) {
        if ($i ~ /[\t\r\n\0]/) { print "ERROR: control char in field " i " at line " NR > "/dev/stderr"; exit 1 }
      }
      print
    }' \
  > "$TMP_MANIFEST" || { rm -f -- "$TMP_D" "$TMP_L" "$TMP_F" "$TMP_MANIFEST" "$TMP_SHA"; exit 1; }
rm -f -- "$TMP_D" "$TMP_L" "$TMP_F"

# === Step 4：NF=4 二次硬约束（v8 强化：直接 awk 计数） ===
NF_BAD=$(LC_ALL=C awk -F'\t' 'NF != 4 { print NR": "$0 }' < "$TMP_MANIFEST" | wc -l)
if [ "$NF_BAD" -ne 0 ]; then
  echo "ERROR: tree-manifest 字段数 ≠ 4（$NF_BAD 行）" >&2
  exit 1
fi

# === Step 5：atomic manifest + sha256 commit（v9 per codex v8 P1 #5） ===
# v8 顺序：先 mv manifest → 再算 hash → 再 mv sha256。任一步失败：
#   - 若 sha256sum 失败：manifest 已覆盖旧文件，旧 hash 仍指向旧内容 → 不匹配
#   - 若第二次 mv 失败：同上 + 输出对不一致
# v9 顺序：先对 TMP_MANIFEST 计算 hash（保证 hash 与 TMP_MANIFEST 内容匹配），
#   但 .sha256 中固定写入最终 basename（保证 rename 后 `sha256sum -c` 通过）；
#   最后才原子 rename 两个临时文件。任一 rename 失败 → trap 清理临时文件，
#   最终路径状态：要么两个文件都是新、要么两个文件都是旧，**不可能错位**。
LC_ALL=C sha256sum "$TMP_MANIFEST" \
  | LC_ALL=C awk -v f="d-stage-audit.${LABEL}.manifest.tsv" '{print $1"  "f}' \
  > "$TMP_SHA"
mv -f -- "$TMP_MANIFEST" "${OUT_DIR}/d-stage-audit.${LABEL}.manifest.tsv"
mv -f -- "$TMP_SHA" "${OUT_DIR}/d-stage-audit.${LABEL}.manifest.tsv.sha256"

trap - EXIT
echo "OK: ${OUT_DIR}/d-stage-audit.${LABEL}.manifest.tsv ($(LC_ALL=C wc -l < "${OUT_DIR}/d-stage-audit.${LABEL}.manifest.tsv") 行) + .sha256"
```

**调用方式（v7 per codex v6 P1 #3 修订；D 与 D_stage **两次独立调用**，不再"同上"）**：

```bash
# 调用 D（输入 lock 1）
tools/d-stage-audit-gen.py gen-manifest \
  --d-root /path/to/migration/supervised-source-tree/<D 子路径> \
  --d-stage-root /tmp/r16-d-stage \
  --out-dir docs/planning/evidence \
  --label D

# 调用 D_stage（输入 lock 2；**独立调用，参数明确，不复用 D 的状态**）
tools/d-stage-audit-gen.py gen-manifest \
  --d-root /path/to/migration/supervised-source-tree/<D 子路径> \
  --d-stage-root /tmp/r16-d-stage \
  --out-dir docs/planning/evidence \
  --label D_stage
```

**v8 命令关键变化（per codex v7 P1 #2 + P1 #3 + P1 #4 + P1 #5）**：

- **类型分支**（v7 per codex v6 P1 #3）：`find -type d` / `find -type l` /
  `find -type f` 三次独立调用，**完全替代** v6 `find -printf '%y\t%P\t'
  -exec readlink {} \; -print`（后者对 f 类型会产生额外 readlink 输出，
  且 `SRC_ROOT` 未定义）；
- **NUL 安全**（v7 per codex v6 P1 #3 + **v8 per codex v7 P1 #3 扩展**）：
  find 输出 `\0` 结尾，`xargs -0` 解析，sort `-z` 排序，避免路径含空格 /
  换行 / 中文 的字节截断；**v8 扩展**：最终 TSV 写入前逐行 awk 校验
  filename / symlink_target / file_sha256 字段不含 tab/CR/LF/NUL 等
  控制字符（避免破坏 TSV 字段边界）；
- **SRC_ROOT 显式 export 到 xargs 子进程**（**v8 per codex v7 P1 #2**）：
  v7 写法 `xargs -0 -I {} sh -c '...'` 因 `sh -c` 子进程不继承调用方
  shell 局部变量，sha256sum 路径失败 → exit 123；v8 改用
  `env SRC_ROOT="$SRC_ROOT" bash -c '...'` 显式 export + bash 子进程
  内置防御性 `if [ -z "$SRC_ROOT" ]` 校验（缺失立即 exit 78）；
- **readlink 失败立即 exit**（**v8 per codex v7 P1 #3**）：`if ! target=$(...)`
  → exit 5（input read error）；v7 `readlink ... || echo ''` 吞错路径
  会把错误 target 写入 TSV 字段，破坏下游分类；
- **每步 rc 检查**（v7 + v8 保留）：`set -euo pipefail` + 关键步骤
  `|| { ...; exit N; }`（sha256sum 失败 exit 5，sort 失败 exit 1）；
- **临时文件**（v7 + v8 保留）：所有产物先写 `.tmp.$$`，原子
  `mv -f` 到最终路径；rename 失败立即 exit；
- **validate-then-replace**（**v8 per codex v7 P1 #5 修订**）：v8 **删除**
  v7 "旧产物保护（先 backup 到 .bak.<timestamp>）"逻辑——v7 旧逻辑在校验
  前 backup，校验失败时新产物未生成但旧产物已被覆盖（路径冲突 + 同秒
  重跑覆盖 backup）；v8 改为：全部校验（NF=4 + 字段编码 + sha256 +
  行数）在临时文件上完成，仅全部通过后才 `mv -f` 到最终路径；最终路径
  已存在时直接覆盖（git 跟踪文件历史由 git 自身负责）；
- **D 与 D_stage 两次独立调用**（v7 + v8 保留）：每次显式指定
  `--label D` 或 `--label D_stage`，不再"同上"占位；
- **工具版本校验**（v7 + v8 保留）：启动时校验 `findutils ≥ 4.9`，
  不符合 exit 78（EX_CONFIG）；
- **字段数硬约束**（v7 + v8 强化）：生成合并时先 awk 校验 NF=4 + 字段
  编码，最终合并后再次 awk 计数 NF≠4 行数（双重校验），不符合 exit 1。

### 4.4 tree-manifest 不变量（v8 per codex v7 P1 #1 删除 F-only 误入项）

- **确定性**：同一源树两次生成 manifest 字节一致（SHA-256 匹配）；
- **完整性**：必须覆盖源树根与**所有**条目（files + directories + symlinks），
  无 hidden / 无 missing；
- **可重放**：输出可被 git 长期跟踪 + 第三方独立验证（仅需 find + sha256sum +
  LC_ALL=C + sort + xargs）；
- **不含 F0 路径**：manifest 仅记录 D 与 D_stage 源树根的路径，**不得**
  包含任何 F0 命名空间（per §九 F0 reference violation，**v8 per codex v7 P1 #1
  保留**：脚本启动时检查 `--d-root` / `--d-stage-root` 不含 F0 命名空间
  模式，匹配则 exit 4）；
- **不含 F-only 误入**（**v8 per codex v7 P1 #1 删除**：F-only 检测在 O-2
  不可计算；F-only 文件出现在 D 或 D_stage 源树中**不**作为 O-2 失败条件）；
  F-only 检测整体移到阶段三 O-4 闭合后（详 §十三 v8）。

### 4.5 D 源树根 + D_stage 源树根（输入路径，v8 per codex v7 P1 #1 修订）

| 输入 | 说明 | 路径 | 锁定方式 |
| --- | --- | --- | --- |
| **D 源树根** | Deepin 4.0.x 原始快照 | `migration/supervised-source-tree/` 内由 dsh 指定的 D 子路径（**绝对保护区** per `docs/project/multiagent-collab.md:40-41`；脚本只读） | tree-manifest SHA-256（`d-stage-audit.D.manifest.tsv.sha256`） |
| **D_stage 源树根** | 阶段一输出的 Deepin-derived staging 树 | **`/tmp/r16-d-stage/`**（v24 唯一合法路径；系统 `$TMPDIR`，**不在仓库任何路径下**） | tree-manifest SHA-256（`d-stage-audit.D_stage.manifest.tsv.sha256`） |

**前置条件**：O-1 全闭合（19 项台账全部展开为 Deepin 中性变更记录 +
来源 patch / 顺序 / 阶段一终判字段填实）+ `/tmp/r16-d-stage/` 物化就位后才能启动
O-2 实际生成。

**关键不变量（v8 per codex v7 P1 #1 强化）**：

- **不得读取 F0**（路径 / SHA / 内容 / 命名空间任何引用均禁止；脚本启动
  时主动检查输入路径不含 F0 命名空间）；
- **不得进行 F-only 检测**（per codex v7 P1 #1：v7 通用规则因不读 F0 +
  不持有 F-only 文件清单而不可计算，v8 整体删除）；F-only 检测全部移到
  阶段三 O-4 闭合后；
- **不得引用 P5 23 BC 矩阵 / 435 per-file 证据**（v7 per codex v6 P1 #2）：
  O-2 阶段一输入仅含 D + D_stage，**不引用** P5 任何 per-file 列表；
- **不得引用阶段三产物**（030-NNN / O_stage / 阶段三 fixture 等）；
- 仅在 D 端域内做完整性审计。

## 五、生成脚本占位

**脚本路径**：`tools/d-stage-audit-gen.py`（**非保护区路径**，`tools/` 不在
保护区清单内）。

**当前状态（v24）**：脚本**仍为占位**（v5 占位升级，v24 接口扩展），未实际生成；待 O-1 全闭合 +
D / D_stage tree-manifest 锁定后由 qoder 实现并 codex 复审。

**脚本接口（v24 占位规范，per codex v9 P1 #5 扩展 + codex v10 P1 #1+#2+#3+#4 + codex v11 P1 #1+#2 + codex v11 P2 #4 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2 修订）**：

```python
# tools/d-stage-audit-gen.py（v24 占位，待 O-1 全闭合 + tree-manifest 锁定后实现）
#
# 唯一可执行入口：tools/d-stage-audit-gen.py <subcommand> [args]
#
# 三个子命令（v24 per codex v9 P1 #5 扩展 + codex v10 P1 #1+#2+#3+#4 修订 + codex v11 P1 #1+#2 进一步修订 + codex v11 P2 #4 退出码作用域契约 + codex v12 P1 #1-#4 journal 协议修订 + codex v13 P1 #1+#2 + P2 #3 排他锁/路径约束/旧对完整性修订 + codex v14 P1 #1+#2 锁时序/事实一致性修订 + codex v15 P1 #1+#2 旧对指纹/reconcile 子命令入口修订 + codex v16 P1 #1+#2 + P2 #3 共享分类/状态不变量/旧对自洽修订 + codex v17 P1 #1+#2 + P2 #3 幂等 no-op/selfcheck fail-closed/16 场景计数修正修订 + codex v18 P1 #1+#2 + P2 #3 fsync 屏障/Python 契约/no-op 严格 sidecar 校验/22 场景修订 + codex v19 P1 #1 + P2 #2 跨目录 rename 双目录同步/3 断电窗口场景/25 场景/reconcile 标签统一修订 + codex v20 P1 #1+#2+#3 rolling_back 白名单/无 journal 走全新路径/verified 清理 .txn 屏障/28 场景修订 + codex v21 P1 #1 白名单可执行等价判定修订 + codex v22 P1 #1 回滚还原逐次 mv 屏障修订 + codex v23 P1 #1+P2 #2 回滚段 fsync 退出码统一 exit 5/回滚还原 fsync 窗口三态契约场景 29 场景修订）：
#   1) gen-manifest  →  生成 9 文件 O-2 输出（主表 + 专表 + genesis + 2 manifest + 4 .sha256）
#   2) snapshot      →  生成 d-stage-snapshot.tar.zst + .sha256（含排他锁 + staging + reconcile-first + 持久化事务目录 + 结构化 journal 协议 + staging_dir 路径约束 + 旧对完整性 fail-closed + 故障注入测试要求）
#   3) reconcile     →  独立 reconcile 入口（对已存在的 tarball + manifest 做 4 字段校验）
#
# 子命令 1：gen-manifest
#   输入：
#     --d-root         <path>   D 源树根（**只读**；不得修改；v7 per codex v6 P1 #1
#                                默认 = `migration/supervised-source-tree/` 内 dsh 指定子路径）
#     --d-stage-root   <path>   D_stage 源树根（v24 唯一合法路径 = `/tmp/r16-d-stage/`，
#                                系统 $TMPDIR，不在仓库任何路径下）
#     --out-dir        <path>   输出目录（必须为非保护区；v7 默认
#                                docs/planning/evidence/）
#     --label          <D|D_stage>  v7 per codex v6 P1 #3：标识当前调用生成哪个 manifest；
#                                       两次独立调用（--label D 与 --label D_stage）
#   输出（v24 共 8 文件 + 1 运行时元数据；八件套 + 各自 sha256 + 1 个 genesis.json 运行时元数据，
#   详 §十一 v24）：
#     <out-dir>/d-stage-audit.tsv                  主表：6 字段 schema 锁定
#     <out-dir>/d-stage-audit.tsv.sha256           SHA-256 of 主表
#     <out-dir>/d-stage-audit.symlink.tsv          专表：7 字段 schema（9 类 symlink 闭合；v10
#                                                  per codex v9 P1 #4 canonicalize 强化）
#     <out-dir>/d-stage-audit.symlink.tsv.sha256   SHA-256 of 专表
#     <out-dir>/d-stage-audit.genesis.json         生成元数据（v24 不含 f_only_exclusion_check 字段，
#                                                  整体 F-only 检测已移到阶段三；详 §十一 v24；
#                                                  v11 per codex v10 P1 #3 新增 tool_versions.jq +
#                                                  fault_injection_tests 字段）
#     <out-dir>/d-stage-audit.D.manifest.tsv       D 源树 tree-manifest（输入 lock 1）
#     <out-dir>/d-stage-audit.D.manifest.tsv.sha256
#     <out-dir>/d-stage-audit.D_stage.manifest.tsv       D_stage 源树 tree-manifest（输入 lock 2）
#     <out-dir>/d-stage-audit.D_stage.manifest.tsv.sha256
#   退出码（v24 per codex v11 P2 #4：子命令作用域内解释）：
#     0  = 成功
#     1  = schema violation（6/7/4 字段 NF≠预期 / 字段编码）
#     2  = determinism violation（两次 SHA-256 不一致）
#     3  = protected path violation
#     4  = F0 reference violation（**仅** gen-manifest 作用域；snapshot 子命令的 exit 4 = reference manifest 缺失或不可访问，详 snapshot 退出码表）
#     5  = input SHA-256 mismatch（D / D_stage tree-manifest 与 genesis 不一致）**仅 gen-manifest 作用域**
#     7  = symlink 闭合失败
#     8  = SYM-N-A reconcile 失败
#     78 = EX_CONFIG（LC_ALL≠C / 工具版本不符）
#   （v8 per codex v7 P1 #1 删除原 F-only violation 退出码 6；F-only 检测整体移到阶段三 O-4）

#
# 子命令 2：snapshot
#   输入（v24 per codex v11 P1 #2 修订：CLI 参数解析必须实际生效；
#         v24 per codex v11 P2 #4：退出码仅在子命令作用域内解释）：
#     --d-stage-root   <path>   D_stage 源树根（必填 = /tmp/r16-d-stage/）
#     --out-dir        <path>   输出目录（必填 = docs/planning/evidence/4.0.2-i3/）
#     [--reference-manifest <path>]（v24 修订：**可选**参数；有默认值）
#                              reference manifest 路径（v24 CLI 默认：
#                                <out-dir>/../d-stage-audit.D_stage.manifest.tsv
#                                即 gen-manifest --label D_stage 的输出之一；
#                                snapshot 必须基于已锁定的 D_stage 输入 lock 2
#                                才能 reconcile PASS）。
#                                调用方可显式覆盖（两种形式都支持：
#                                --reference-manifest=<path> 或
#                                --reference-manifest <path>）；任一情况下
#                                工具先 `realpath -m` canonicalize 传入路径或
#                                默认路径，再校验文件存在且为 regular file。
#                                v24 删除 v11 "必须显式传递" 措辞（v12 起，
#                                与"可选"标记矛盾，per codex v11 P1 #2）。
#   输出（v24 per codex v9 P1 #2 + P1 #3 + codex v10 P1 #1 + codex v11 P1 #1 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2：
#         目录级 staging + reconcile-first + 排他锁 + 持久化事务目录 +
#         结构化 journal 协议 + 三者事实一致性 + 幂等 no-op + fsync 状态
#         依赖数据屏障（跨目录 rename 源、目标双目录同步；verified 清理
#         .txn 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行
#         等价判定）+ 无 journal 恢复走全新路径 + 故障注入测试要求）：
#     <out-dir>/d-stage-snapshot.tar.zst          D_stage 完整快照
#     <out-dir>/d-stage-snapshot.tar.zst.sha256   SHA-256 锁定
#   内部流程（per §5.3 v24 030-design 排他锁 + 持久化事务目录 + 结构化 journal 协议）：
#     0. **OUT_DIR 排他锁**（v24 per codex v13 P1 #1 + codex v14 P1 #1）：
#        fcntl.flock LOCK_EX|LOCK_NB 于 ${out-dir}/.snapshot.lock（锁文件
#        永不删除）；**锁在参数解析之后、任何文件系统变更之前获取**
#        （先于临时文件、.txn 创建、staging 生成、reconcile）；获取失败 →
#        exit 6（不读 journal、不动任何文件）；锁覆盖整个运行期，kill -9
#        后由内核随 fd 关闭释放
#     1. 启动恢复检查：若 ${out-dir}/.txn 存在 → 解析 .txn/journal
#        （schema=1 校验 + state/staging_dir 字段逐行解析；per codex v12
#        P1 #2 结构化格式，严禁整行与裸状态比较）；journal 缺失 → old.*
#        存在则 exit 9（不变式违反），否则 rm -rf .txn 后**继续全新路径**
#        （v21 per codex v20 P1 #2：不再因"OUT_DIR 自洽"直接判为已完成
#        新对 exit 0——无 journal 即无新对指纹证据，旧对可能被误判为新对；
#        "上次已完成"情形由 4a 幂等 no-op 在严格校验下等价收敛）
#     2. **journal 字段严格校验 + staging_dir 路径约束**（v24 per codex
#        v13 P1 #2 + codex v15 P1 #1）：old_tar/old_sha ∈ {present,absent}
#        且一致、new_tar_sha256 = 64 hex、new_tar_size = 数字、
#        old_tar_sha256/old_sha_sha256 = 64 hex 或 none；staging_dir 必须
#        绝对路径 → realpath -m canonicalize → is_within(.txn/staging) →
#        非 symlink → 必须为目录；任一失败 exit 9
#     3. **三者事实一致性校验**（v24 per codex v14 P1 #2 + codex v15
#        P1 #1 指纹严格匹配 + codex v16 P1 #1 共享分类 + codex v17
#        P1 #1 幂等 no-op 前置消除旧新指纹相等歧义 + codex v18 P2 #3
#        no-op 复用严格 sidecar 校验）：journal 计划 +
#        .txn/old.* + OUT_DIR 三者 fail-closed；**共享 classify_out_pair()**
#        分类 OUT_DIR 文件 absent/new/old/unknown——tarball sha256 ==
#        new_tar_sha256 → new、== old_tar_sha256 → old、其余 unknown →
#        exit 9（不得把未知/损坏/替换内容误判为旧文件）；.sha256 恰好
#        一行（wc -l == 1）+ 精确格式 + hash 字段 ∈ {新, 旧} 否则 exit 9；
#        .txn/old.* 内容必须匹配指纹；计划 present → 旧文件恰好存在于
#        .txn 或 OUT_DIR 之一，两者同时存在 / 均不存在 → exit 9（绝不
#        静默恢复成半对）；统一混合判定（new/old 混排 exit 9）**仅对非
#        rolling_back 状态生效**（v21 per codex v20 P1 #1）
#     3b. **状态专属不变量**（v24 per codex v16 P1 #2 + v21 per codex
#        v20 P1 #1）：verified /
#        sha_committed 必须 OUT_DIR 两个文件均 new（半对 → exit 9 且不
#        清理任何证据）；staged 不得出现 new；backed_up sha 必须 absent；
#        tarball_committed tar 必须 new；rolling_back 由**合法中途态白名单**
#        收敛保证——逐文件删除/还原的中间事实态 (new,new)/(absent,new)/
#        (absent,absent)/(old,absent)/(old,old) 显式允许并落入回滚段幂等
#        重入，其余组合 exit 9 不清理证据；白名单判定**必须用集合成员
#        判断（Python tuple/set，见 §五 v24 Python 实现契约
#        rolling_back_mid_state_ok），禁止照抄 shell case 未转义 | 语法**
#        （v22 per codex v21 P1 #1：未转义 | 是模式分隔符，v21 草图
#        实际只匹配单个 token）
#     4. 创建固定事务目录 ${out-dir}/.txn/staging（**无 PID 命名**，per
#        codex v12 P1 #1；kill -9 后新进程可从同一路径找回新文件）
#     5. 在 staging 内生成 tarball + sha256
#     6. staging 内 sha256sum -c 自校验
#     7. staging 内 reconcile（解压 + 4 字段 manifest + diff -u reference）
#     8. **旧对完整性 fail-closed + 自洽校验 + 幂等 no-op**（v24 per
#        codex v13 P2 #3 + codex v16 P2 #3 + codex v17 P1 #1 + codex v18
#        P2 #3）：OUT_DIR
#        既有 tarball 与 .sha256 必须"两者
#        都在或都不在"，半对 → exit 9；旧对存在时 .sha256 必须恰好一行 +
#        精确格式 + hash 字段 == 旧 tarball SHA-256 + 整体 sha256sum -c
#        通过（不自洽旧对不得进入事务，损坏证据不被静默丢弃；本校验块是
#        sidecar 严格 schema 的唯一判定入口）；no-op 判定**复用本块结论**
#        （恰好一行 + 精确格式 + hash 字段 + sha256sum -c 通过后，仅比较
#        旧 tarball SHA == 新快照 SHA，确定性重跑）→ 清理 staging 成功
#        返回 exit 0；裸 sha256sum -c 不得作为 no-op 判定（两行重复合法
#        行会通过 -c 但违反严格 schema，per codex v18 P2 #3）
#     8b. **staging 数据持久化屏障**（v19 per codex v18 P1 #1）：
#        persist_journal(state=staged) 之前先 fsync_file(staging tar) +
#        fsync_file(staging sha) + fsync_dir(staging dir)；任一失败 → exit 5
#        （journal 未写，清理 .txn，旧对未动）。kill -9 不丢 page cache，
#        此屏障针对**断电**语义（断电后 state=staged 必须搭配可用的
#        staging 文件，否则下次启动只能 exit 9）
#     9. reconcile PASS 后 persist_journal(state=staged)（含 staging_dir +
#        新文件 sha256/size 指纹 + old_tar/old_sha 备份计划 + **旧对精确
#        指纹 old_tar_sha256 / old_sha_sha256**（v24 per codex v15 P1 #1）；
#        **先于任何旧文件移动**，per codex v12 P1 #3）
#     10. 备份旧对 → .txn/old.tar.zst + .txn/old.sha256（守卫幂等）→
#         fsync_dir(.txn) + fsync_dir(OUT_DIR)（v19 per codex v18 P1 #1 +
#         v20 per codex v19 P1 #1：跨目录 rename 源、目标目录都同步）→
#         persist_journal(state=backed_up)
#     11. 提交新 tarball → fsync_dir(OUT_DIR)（v19 per codex v18 P1 #1）→
#         persist_journal(state=tarball_committed)
#     12. 提交新 sha256 → fsync_dir(OUT_DIR)（v19 per codex v18 P1 #1）→
#         persist_journal(state=sha_committed)
#     13. 最终自校验：PASS → persist_journal(state=verified) → 清理
#         （rm old → fsync_dir(.txn)（v21 per codex v20 P1 #3，先于 rm
#         journal）→ rm journal → rm -rf .txn；**不 rm .snapshot.lock**）→
#         fsync_dir(OUT_DIR)（v19 per codex v18 P1 #1，清理目录项持久）；
#         FAIL → persist_journal(state=rolling_back) → 回滚段（内容感知
#         rm 新文件 + **每个**还原 mv 后立即 fsync_dir(OUT_DIR) +
#         fsync_dir(.txn)（v20 per codex v19 P1 #1 跨目录 rename 源、目标
#         都同步 + v23 per codex v22 P1 #1 逐次 mv 屏障）+ 清理 + 再次
#         fsync_dir(OUT_DIR)（v19 per codex v18 P1 #1）；
#         mv 失败 exit 9 + journal 重入收敛；**每个**还原 mv 后的 fsync_dir
#         失败 exit 5（journal 保持 rolling_back，下次启动重入收敛；
#         v24 per codex v23 P1 #1 统一退出码契约），绝不静默产生半对
#         per codex v14 P1 #2）
#     14. journal 持久化 = 文件 fsync + atomic rename + 目录 fsync（python3
#         helper，per codex v12 P1 #4；严禁只用全局 sync）；v19 per codex
#         v18 P1 #1 新增 fsync_file / fsync_dir helper 构成**状态依赖数据
#         屏障**（顺序见 §五 v24 Python 实现契约，不可交换；任一失败
#         exit 5，journal 保持前一持久状态）
#     15. 故障注入测试（v24 per codex v13 P1 #1+#2 + P2 #3 + codex v14
#         P1 #1+#2 + codex v15 P1 #1 + codex v16 P1 #1+#2 + P2 #3 +
#         codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 +
#         codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 +
#         codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2 共
#         29 场景（v14=11/v15=12/v16=13/v17=16/v18=16/v19=22/v20=25/
#         v21=28/v22=28/v23=28/v24=29；v24 新增 1 个场景：回滚还原 mv 与
#         逐次双目录 fsync 之间断电窗口 per codex v23 P2 #2，并统一回滚段
#         逐次 fsync 失败退出码 exit 5 per codex v23 P1 #1；v23 无新增
#         场景，仅修订回滚还原逐次 mv 屏障与
#         power_loss_rollback_mid_restore_window 期望确定性 per
#         codex v22 P1 #1；v22 无新增场景，仅修订 rolling_back 白名单
#         可执行等价判定 per codex v21 P1 #1；v21 新增 3 个场景
#         （rolling_back 中途删除窗口 / rolling_back 中途还原窗口 /
#         verified 清理 journal 删除后 .txn 残留窗口）；v20 新增 3 个
#         跨目录 rename 断电窗口场景（备份 mv 后 persist 前 / 备份 mv
#         与双目录 fsync 之间 / 回滚还原 mv 后清理前）；v19 新增 5 个
#         power-loss 场景 + 1 个 no-op 重复行 sidecar 场景；
#         power-loss 注入 = 强制断电（VM poweroff / 拔盘），**不得用
#         kill -9 代替**——kill -9 不丢 page cache，覆盖不了断电语义）：
#         锁争用（锁先于一切变更）、staged 落盘后崩溃、备份两
#         mv 之间崩溃、backed_up 落盘后崩溃、tarball mv 与 journal 之间
#         崩溃、自校验失败回滚、OUT_DIR 只读、journal 写入失败（旧对未
#         动）、journal 损坏、staging_dir 路径穿越/symlink、OUT_DIR 既有
#         半对、旧对备份丢失、OUT_DIR 未知内容（损坏/替换）、即时回滚
#         sidecar 未知、verified 状态半对、初始旧对不自洽、power-loss
#         staged 持久化后断电、power-loss staged 持久化前断电、power-loss
#         backed_up 后断电、power-loss tarball mv 后断电、power-loss
#         verified 清理前断电、no-op 前 sidecar 重复行、power-loss 备份
#         mv 后 persist 前断电、power-loss 备份 mv 与双目录 fsync 之间
#         断电、power-loss 回滚还原 mv 后清理前断电、power-loss 回滚
#         中途删除窗口、power-loss 回滚中途还原窗口、power-loss 回滚
#         还原 mv 与逐次双目录 fsync 之间断电窗口、power-loss verified
#         清理 journal 删除后 .txn 残留窗口；验证最终路径
#         状态 = 旧对或新对（不存在错位）；测试结果写入 genesis.json
#         `fault_injection_tests` 字段
#   退出码（v24 per codex v11 P2 #4：**仅在 snapshot 子命令作用域内解释**；
#         严禁与 §十一 全局错误码表跨子命令对照）：
#     0  = 成功（tarball + sha256 + reconcile 4 字段匹配 + 自校验 PASS + 清理完成）
#     1  = staging 内 sha256sum -c 失败
#     2  = reconcile 失败（4 字段比对不一致）
#     3  = protected path violation
#     4  = reference manifest 缺失或不可访问（canonicalize 后不存在）
#     5  = persist_journal 写入失败或备份 mv 失败（journal 保持前一个持久
#          状态，可恢复；旧对不丢失 per codex v12 P1 #3；修复后重跑自动续跑）
#     6  = OUT_DIR 排他锁获取失败（另一 snapshot 进程持有 .snapshot.lock；
#          per codex v13 P1 #1 新增；调用方稍后重试）
#     9  = journal 不可解析 / state 未知 / journal 字段取值非法 / staging_dir
#          路径约束失败（非绝对 / 越界 / symlink / 非目录）/ 三者事实一致性
#          失败（旧文件双存在 / 均缺失 / OUT_DIR 未知内容 / 新旧混合）/
#          .txn 不变式违反（old.* 存在但 journal 缺失）/ staging 新文件指纹
#          不一致 / OUT_DIR 既有快照半对 fail-closed（per codex v12
#          P1 #1+#2 + codex v13 P1 #2 + P2 #3 + codex v14 P1 #2 + codex
#          v15 P1 #1；要求 dsh 人工清理）
#     78 = EX_CONFIG（LC_ALL≠C / tar/zstd 版本不符 / OUT_DIR 文件系统不保证 atomic rename）
#   调用示例（v24）：
#     tools/d-stage-audit-gen.py snapshot \
#       --d-stage-root /tmp/r16-d-stage \
#       --out-dir docs/planning/evidence/4.0.2-i3 \
#       --reference-manifest docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv
#     （不传 --reference-manifest 时使用默认 <out-dir>/../d-stage-audit.D_stage.manifest.tsv；v24 显式声明可选）

# 子命令 3：reconcile
#   输入：
#     --tarball        <path>   已存在的 d-stage-snapshot.tar.zst
#     --manifest       <path>   reference manifest（d-stage-audit.D_stage.manifest.tsv）
#   输出：stdout 报告；不写文件（只读不写）
#   内部流程（per §5.3 v24 030-design reconcile 子命令规范；独立 bash 入口
#             已废弃 per codex v15 P1 #2，唯一合法入口 = 本子命令）：
#     1. 解压 tarball 到临时目录
#     2. 重建 4 字段 manifest
#     3. diff -u reference vs new
#   退出码：
#     0  = 4 字段全部匹配
#     1  = NF≠4 / 字段编码错误
#     2  = reconcile 失败（4 字段比对不一致）
#     3  = tarball / manifest 文件不存在
#
# v24 接口总览表：
# | 子命令       | 输入                                          | 输出                              | 阶段归属 |
# |--------------|-----------------------------------------------|-----------------------------------|----------|
# | gen-manifest | D / D_stage / out-dir / label                 | 9 文件 O-2 输出（8 稳定 + 1 runtime） | 阶段一   |
# | snapshot     | D_stage / out-dir / reference-manifest         | d-stage-snapshot.tar.zst + .sha256 | 阶段二前 |
# | reconcile    | tarball / manifest                            | stdout 报告                        | 阶段二前 |
#
# 严禁（v10 per codex v9 P1 #5 强化）：
#   - 子命令之外的任何"独立"入口（如直接调 shell 片段生成 snapshot）；
#   - snapshot 子命令跳过 staging 步骤直接写最终路径；
#   - reconcile 子命令只比较 (type, path) 不校验 symlink_target / file_sha256；
#   - 任何子命令跨职责（如 gen-manifest 内做 snapshot）；
#   - 读取任何 F0 路径（含路径前缀 / 文件名清单 / SHA / 命名空间）；
#   - 进行 F-only 检测（per codex v7 P1 #1：v7 通用规则不可计算；F-only 移到阶段三）；
#   - 写入任何保护区路径（drivers/、baselines/、debs/、vendor/、build/、
#     third_party/、migration/supervised-source-tree/ 任何子路径、binary-manifest.json、
#     patches/ 顶层除 030-NNN 阶段三允许项外）；
#   - 引用任何阶段三 fixture / 030-NNN / O_stage。
#
# === v24 Python 实现契约（per codex v18 P1 #2 降级引入 + codex v19 P1 #1
# 跨目录 rename 双目录同步修订 + codex v20 P1 #1+#2+#3 rolling_back 白名单/
# 无 journal 走全新路径/verified 清理 .txn 屏障修订 + codex v21 P1 #1
# 白名单可执行等价判定修订 + codex v22 P1 #1 回滚还原逐次 mv 屏障修订 +
# codex v23 P1 #1+P2 #2 回滚段 fsync 退出码统一 exit 5/回滚还原 fsync 窗口
# 三态契约修订：design §5.3 bash 草案已降级
# 为算法伪代码，本契约是 tools/d-stage-audit-gen.py snapshot 子命令实现的
# 唯一依据；契约与草案冲突时以本契约为准） ===
#
# 执行顺序（与 bash 伪代码线性排版不同，恢复检查**先于** staging 生成）：
#   1) acquire_snapshot_lock(OUT_DIR)             # 失败 exit 6；锁覆盖整个运行期
#   2) 恢复检查（.txn 存在 → journal 解析 / 字段校验 / staging_dir 路径约束 /
#      三者事实一致性 / 状态专属不变量；幂等重入）
#   3) 全新路径：staging 生成 + 自校验 + reconcile
#   4) 4a：旧对完整性 fail-closed + sidecar 严格校验 + 幂等 no-op
#   5) fsync 屏障（见下）→ persist_journal 状态机（staged → backed_up →
#      tarball_committed → sha_committed → verified / rolling_back）
#
# helper 签名（全部 python3；失败按标注退出码退出，严禁全局 sync 替代）：
#   acquire_snapshot_lock(out_dir: Path) -> int
#       # os.open(.snapshot.lock, O_RDWR|O_CREAT, 0o644) + fcntl.flock
#       #   LOCK_EX|LOCK_NB；失败 exit 6；返回 fd（进程退出时内核释放）
#   fsync_file(path: Path) -> None
#       # O_RDONLY + os.fsync + close；失败 exit 5（v19 per codex v18 P1 #1）
#   fsync_dir(path: Path) -> None
#       # O_RDONLY|O_DIRECTORY + os.fsync + close；失败 exit 5（v19 per codex v18 P1 #1）
#   persist_journal(journal: Path, fields: dict) -> None
#       # tmp 写入 + fsync_file(tmp) + os.rename + fsync_dir(parent)；
#       # 失败 exit 5（journal 保持前一持久状态）
#   classify_out_pair(...) -> None
#       # 副作用分类 OUT_TAR_STATE / OUT_SHA_STATE ∈ {absent,new,old,unknown}；
#       # sidecar 判定 = 恰好一行 + 精确格式 + hash 字段 ∈ {新指纹,旧指纹}，
#       # 任一不满足 → unknown
#   sidecar_is_strict(...) -> bool
#       # 恰好一行 + 精确格式 + hash 字段（4a 严格校验与幂等 no-op 共用，
#       # per codex v18 P2 #3；不得用裸 sha256sum -c 代替——两行重复合法
#       # 行会通过 -c 但违反严格 schema）
#   rolling_back_mid_state_ok(tar_state: str, sha_state: str) -> bool
#       # rolling_back 合法中途态白名单（v21 per codex v20 P1 #1 引入 +
#       # v22 per codex v21 P1 #1 修订为可执行等价判定）：
#       #   return (tar_state, sha_state) in {
#       #       ("new", "new"), ("absent", "new"), ("absent", "absent"),
#       #       ("old", "absent"), ("old", "old"),
#       #   }
#       # **禁止照抄 design §5.3 shell 草图的 case 语法**：shell case 中
#       # 未转义的 | 是模式分隔符而非状态对字符串的一部分（v21 草图
#       # `new|new|absent|new|...` 实际只匹配单个 token，五个白名单状态对
#       # 全部落入拒绝分支）；Python 实现必须用 tuple/set 成员判断，
#       # 或以带引号的精确字符串匹配作为 shell 等价形式。
#
# fsync 屏障顺序（v19 per codex v18 P1 #1 + v20 per codex v19 P1 #1；不可
# 交换；任一失败 exit 5，journal 保持前一持久状态）：
#   persist(staged)            ← fsync_file(staging tar) + fsync_file(staging sha)
#                                + fsync_dir(staging dir)（先于任何旧文件移动）
#   persist(backed_up)         ← 备份 mv 后 fsync_dir(.txn) + fsync_dir(OUT_DIR)
#                                （跨目录 rename 必须同步**源、目标两个目录**；
#                                只同步目标目录会在断电后残留 OUT_DIR 源目录项
#                                → 旧文件双存在 → 恢复 exit 9）
#   persist(tarball_committed) ← 新 tar mv 到 OUT_DIR 后 fsync_dir(OUT_DIR)
#                                （staging 源目录残留项由清理 rm -rf .txn 消除，
#                                不影响恢复判定，无需单独同步）
#   persist(sha_committed)     ← 新 sha mv 到 OUT_DIR 后 fsync_dir(OUT_DIR)（同上）
#   persist(verified)          ← sha256sum -c PASS（文件本身已由 fsync 持久）
#   verified 清理              ← rm old.* 后 fsync_dir(.txn)（v21 per codex
#                                v20 P1 #3，先于 rm journal）→ rm journal /
#                                rm -rf .txn 后 fsync_dir(OUT_DIR)
#   回滚段                     ← **每个**还原 mv 后立即 fsync_dir(OUT_DIR) +
#                                fsync_dir(.txn)（源、目标都同步；v23 per codex
#                                v22 P1 #1 逐次 mv 屏障——两次还原 mv 之间断电时
#                                统一 fsync 会导致第一次 mv 的源/目标目录项未
#                                持久 → 旧 tar 双存在或均缺失 exit 9；逐次屏障后
#                                (old,absent) 由持久化保证；v24 per codex v23
#                                P1 #1 任一 fsync 失败 exit 5 统一退出码契约，
#                                journal 保持 rolling_back 重入收敛），
#                                清理后再次 fsync_dir(OUT_DIR)
```

**脚本不变量（v8 per codex v7 P1 #1 + P1 #5 + P1 #6 规范）**：

- 输入仅依赖 tree-manifest 锁定的 D 与 D_stage 路径（**不引用** P5 435
  per-file / 23 BC 矩阵任何 per-file 列表）；
- 输出固定 schema（主表 6 字段 + 专表 7 字段 + tree-manifest 4 字段，详 §十二 / §四 / §八.5）；
- 不可手填：禁止 qoder / dsh 直接编辑 d-stage-audit.tsv 内容；
- 可复现：相同输入两次运行结果字节一致（**8 稳定文件 SHA-256 自校验通过**；
  genesis.json **不计入** SHA 一致判定，per codex v7 P1 #6）；
- 完整性：必须覆盖 D 与 D_stage 间**所有**文件级差异（不限制具体条数）；
- **不得读取任何 F0 路径**（即使该路径在系统上存在；启动时主动校验）；
- **不得写入任何保护区路径**（drivers/、baselines/、debs/、vendor/、
  build/、third_party/、migration/supervised-source-tree/ 任何子路径、
  binary-manifest.json、patches/ 顶层除 030-NNN 阶段三允许项外）；
- **不得进行 F-only 检测**（per codex v7 P1 #1：v7 通用规则因不读 F0 +
  不持有 F-only 文件清单而不可计算；F-only 检测整体移到阶段三 O-4 闭合后）；
- **不得**引用 P5 任何 per-file 列表（435 证据 / 23 BC 矩阵）；阶段一
  O-2 仅做 D vs D_stage 完整性闭合，**不引用** F0 比较矩阵任何字段；
- **9 文件输出含 2 输入 lock + 各自 sha256**（per codex v6 P1 #5），
  阶段二 release commit 必须包含全部 9 文件（详 §十一 + 框架 §五.2 v7）。

## 六、工具链规范（v6 新增 findutils）

| 工具 | 用途 | 版本约束 | 验证命令 |
| --- | --- | --- | --- |
| **diffutils** | 文件级 diff（diff -ruN --brief） | GNU diffutils ≥ 3.8（支持 --brief + -u） | `diff --version \| head -1` |
| **git** | stat + per-file hash 校验（仅用于交叉验证，非 diff 主源） | git ≥ 2.30 | `git --version` |
| **python3** | 聚类脚本 + schema 校验 + SHA-256 计算 + genesis 写入 | python3 ≥ 3.11（dict ordering 稳定） | `python3 --version` |
| **sha256sum** | D / D_stage / 输出物 hash 锁定 | coreutils ≥ 8.32 | `sha256sum --version \| head -1` |
| **findutils** | **tree-manifest 枚举**（find -mindepth 0 -printf '%y\t%P\t' + -exec readlink） | **findutils ≥ 4.9**（v6 新增；支持 -printf 全部占位符） | `find --version \| head -1` |
| **LC_ALL=C** | 全程 locale 锁定（避免 UTF-8 排序差异） | 环境变量 | `echo $LC_ALL` |

**版本记录位置**：`d-stage-audit.genesis.json` 的 `tool_versions` 字段记录
全部工具版本号（v6 含 findutils）；任何工具版本变更必须重新生成 D → D_stage
完整性审计（SHA-256 不再匹配视为不可比）。

**LC_ALL=C 强制**：生成脚本启动时校验 `os.environ['LC_ALL'] == 'C'`，否则
exit 78（EX_CONFIG）失败关闭。

## 七、排序归一化（v7 强化 tree-manifest + 9 类 symlink）

**目的**：消除文件遍历顺序的随机性，确保 deterministic replay。

**实现**：

1. **tree-manifest 排序**（v7 per codex v6 P1 #3 修订）：`find -type d` /
   `find -type l` / `find -type f` **三次独立枚举** + NUL 安全 + xargs 字节
   安全，输出按 `(file_type, relative_path)` 二级 LC_ALL=C 字节序排序；
   file_type 字典序 `d < f < l`（C locale 下 ASCII：100 < 102 < 108）；
2. **文件枚举排序**：`diff -ruN --brief` 输出按 `d_rel`（缺则空字符串）
   字段 LC_ALL=C 字节序排序；同 `d_rel` 时按 `d_stage_rel` 字段排序；
3. **分类排序**：分类结果（differs / identical / D-only / D_stage-only +
   **9 类** symlink 细分，per §八.1 v7）按上述排序键 LC_ALL=C 排序；
4. **最终输出排序**：
   - 主表 6 字段按 `classification` → `d_rel` → `d_stage_rel` 三级排序
     键 LC_ALL=C 排序；
   - 专表 7 字段按 `classification` → `d_rel` → `d_stage_rel` 三级排序
     键 LC_ALL=C 排序（**9 类** class 顺序：same-target / target-changed /
     single-side-d / single-side-dstage / d2dstage / dstage2d / dangling /
     broken / loop，per §八.1 v7）。

**禁止**：使用 Python `sorted()` 默认（locale-aware）；必须显式
`key=lambda x: x.encode('utf-8')` + `sorted(..., cmp=...)` 或
`locale.strcoll` 替换为字节比较。

## 八、symlink 处置（v7 9 类细分 + 互斥判定 + SYM-N-A reconcile）

> **v7 修订（per codex v6 P1 #4）**：v6 标题与 schema 称 8 类，但表格
> 实际列出 9 类（same-target / target-changed / single-side-d /
> single-side-dstage / d2dstage / dstage2d / dangling / broken / loop），
> 且跨树（d2dstage / dstage2d）放在 same-target / target-changed 之后
> 判定，导致跨树分类与路径变更分类**不互斥**。v7 改为**显式 9 类
> 互斥判定**，跨树作为与 same-target / target-changed **并列**的独立
> 分类（不是 state field）；priority 1-4 排除 single-side / dangling /
> broken / loop 后，priority 5-8 在两端均存在的剩余条目上判定
> **跨树（target 在对方子树内）** → d2dstage / dstage2d，
> **同 target** → same-target，**不同 target** → target-changed。

### 8.1 9 类 symlink 分类（v7 互斥且完整；含跨树独立分类）

| # | 分类 | 含义 | D 端存在 | D_stage 端存在 | target 一致 | 跨树指向 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | **`symlink-same-target`** | 两端 symlink 指向**相同** target（target 不在对方子树内） | ✓ | ✓ | ✓ | ✗ |
| 2 | **`symlink-target-changed`** | 两端 symlink 指向**不同** target（两个 target 均不在对方子树内） | ✓ | ✓ | ✗ | ✗ |
| 3 | **`symlink-single-side-d`** | 仅 D 端存在，D_stage 端不存在（删除） | ✓ | ✗ | — | — |
| 4 | **`symlink-single-side-dstage`** | 仅 D_stage 端存在，D 端不存在（新增） | ✗ | ✓ | — | — |
| 5 | **`symlink-d2dstage`** | D → D_stage 指向（D 端 symlink target 路径在 D_stage 子树内） | ✓ | ✓ | — | ✓ |
| 6 | **`symlink-dstage2d`** | D_stage → D 指向（D_stage 端 symlink target 路径在 D 子树内） | ✓ | ✓ | — | ✓ |
| 7 | **`symlink-dangling`** | target 路径不存在 | — | — | — | — |
| 8 | **`symlink-broken`** | target 存在但非 file/dir/symlink 或权限拒绝 | — | — | — | — |
| 9 | **`symlink-loop`** | target 路径形成环（D→A→D 或 D→A→B→A） | — | — | — | — |

### 8.2 互斥判定优先级（v7 per codex v6 P1 #4 修订 + **v9 per codex v8 P1 #6 补全跨树判定定义 + v10 per codex v9 P1 #4 canonicalize 强化**）

**v9 per codex v8 P1 #6 关键补充**：

v8 引用了 `target_d_abs` / `target_dstage_abs` 但未定义，跨树判定
（priority 6-7）无法实现。v9 显式定义：

- **`target_d_abs`（D 端 symlink target 绝对路径）**：
  - 若 `target_d`（readlink 原始字符串）以 `/` 开头（绝对 target）：
    `target_d_abs = target_d`（直接采用，无需进一步解析）；
  - 若 `target_d` 不以 `/` 开头（相对 target）：
    `target_d_abs = os.path.normpath(os.path.join(dirname(symlink_d), target_d))`
    即"以 symlink 所在目录为基准 + target 相对路径拼接 + 规范化"；
  - bash 实现：
    ```bash
    if [ "${target_d:0:1}" = "/" ]; then
      target_d_abs="$target_d"
    else
      target_d_abs="$(cd "$(dirname "$symlink_d")" && printf '%s/%s' "$PWD" "$target_d")"
      target_d_abs="$(cd "$(dirname "$target_d_abs")" 2>/dev/null && printf '%s/%s' "$PWD" "$(basename "$target_d_abs")")" \
        || target_d_abs="$(readlink -f -- "$symlink_d")"
    fi
    ```
- **`target_dstage_abs`**：D_stage 端对称定义（基于 `symlink_dstage` 所在目录 + `target_dstage`）；
- **`is_within(path_abs, root_abs)`**：
  - 严格前缀匹配（避免 `/foo/bar` 被 `/foo/ba` 误判）：
    `path_abs == root_abs` OR `path_abs` starts with `root_abs + '/'`；
  - bash 实现：
    ```bash
    is_within() {
      local path_abs="$1" root_abs="$2"
      [ "$path_abs" = "$root_abs" ] \
        || [ "${path_abs:0:${#root_abs}+1}" = "$root_abs/" ]
    }
    ```

**v10 per codex v9 P1 #4 关键修订（canonicalize 强化）**：

v9 实现存在两处错误分类风险：
- **绝对 target 直接采用原始字符串**：若 target 含 `..` 组件（如
  `/tmp/d/../d-stage/x`），`is_within` 字符串前缀匹配会漏判；
- **相对 target 拼接后未规范化最终 `..`**：bash `(cd dir && printf '%s/%s' "$PWD" "$target")`
  仅做物理 `cd` + 字符串拼接，不消除 `..` 残留。

v10 改用 `realpath -m` 对所有路径统一 canonicalize（POSIX realpath 不
保证；GNU coreutils ≥ 8.25 提供 `-m` 选项，不要求路径实际存在，可在
target 缺失 / dangling 场景下使用）：

```bash
# 1. 源树根 canonicalize（启动时一次性）
D_SRC_ROOT_CANONICAL=$(realpath -m -- "$D_SRC_ROOT")
DSTAGE_SRC_ROOT_CANONICAL=$(realpath -m -- "$DSTAGE_SRC_ROOT")

# 2. symlink_d 所在目录 canonicalize（不含 symlink 自身，避免 realpath
#    解析掉 symlink）
symlink_d_dir=$(dirname -- "$symlink_d")
symlink_d_dir_canonical=$(realpath -m -- "$symlink_d_dir")

# 3. target_d_abs 计算（先拼接再 canonicalize）
if [ "${target_d:0:1}" = "/" ]; then
  target_d_joined="$target_d"
else
  target_d_joined="${symlink_d_dir_canonical}/${target_d}"
fi
target_d_abs=$(realpath -m -- "$target_d_joined")

# 4. is_within 在 canonical 路径上做严格前缀匹配
is_within() {
  local path_abs="$1" root_abs="$2"
  [ "$path_abs" = "$root_abs" ] \
    || [ "${path_abs:0:${#root_abs}+1}" = "$root_abs/" ]
}

# 5. 调用
d_target_in_dstage=$(is_within "$target_d_abs" "$DSTAGE_SRC_ROOT_CANONICAL")
```

**v10 canonicalize 覆盖的 fixture**：

| Fixture | target_d 示例 | 旧版（v9）分类 | 新版（v12，自 v10 起）分类 |
| --- | --- | --- | --- |
| 含 `..` 组件的绝对 target | `/tmp/d/../d-stage/x`（应落入 D_stage 子树） | `symlink-target-changed`（漏判） | `symlink-d2dstage`（正确） |
| 含 `..` 组件的相对 target | symlink 在 `/tmp/d/foo`，target `../d-stage/x` | `symlink-dangling`（target 不存在） | `symlink-d2dstage`（正确） |
| target 指向源树根 | `D_SRC_ROOT` 本身 | 取决于前/后 `/` 匹配 | 严格前缀匹配通过 |
| out-of-tree 绝对 target | `/etc/passwd` | `symlink-target-changed` | `symlink-target-changed`（正确） |
| dangling 相对 target | symlink 在 `/tmp/d/foo`，target `nonexistent` | `symlink-dangling` | `symlink-dangling`（正确；realpath -m 不要求存在） |
| 重复 `/` 组件 | `//tmp///d-stage/x` | 字符串前缀不匹配 | canonicalize 后匹配 |

**严禁**：
- 对绝对 target 直接字符串前缀比较（不消除 `..`）；
- 在 D / D_stage 根未 canonicalize 的情况下做 `is_within`；
- 用 `readlink -f` 替代 `realpath -m`（`-f` 要求路径全部存在，dangling
  symlink / 源树中尚不存在的 target 场景下失败；`-m` 不要求存在）；
- 在 canonicalize 之前做 `is_within` 判定（任何 `..` 残留都会破坏前缀匹配）。

完整判定流程（v13 per codex v10 P1 #4 + codex v11 P1 #4 闭环：直接引用 §八.2 同一 canonicalize
实现 — 见上方 symlink canonicalize bash 片段 — 避免重复伪代码与绝对/相对
target 判定冲突；以下伪代码用 `canonicalize_d_target(target_d, symlink_d)`
与 `canonicalize_dstage_target(target_dstage, symlink_dstage)` 表达
canonicalize 函数调用）：

```
对每对 (symlink_d, symlink_dstage)：
  if symlink_d 存在 AND symlink_dstage 不存在:
      → symlink-single-side-d                       (priority 1)
  elif symlink_d 不存在 AND symlink_dstage 存在:
      → symlink-single-side-dstage                  (priority 2)
  else:  # 两端均存在
      target_d      = readlink(symlink_d)            # 原始 target 字符串
      target_dstage = readlink(symlink_dstage)       # 原始 target 字符串
      resolved_d      = realpath(symlink_d)          # 解析所有 symlink
      resolved_dstage = realpath(symlink_dstage)

      if resolved_d 形成环 OR resolved_dstage 形成环:
          → symlink-loop                            (priority 3)
      elif resolved_d 不存在:
          → symlink-dangling                        (priority 4)
      elif resolved_dstage 不存在:
          → symlink-dangling                        (priority 4)
      elif resolved_d 类型异常（不是 file/dir/symlink）或权限拒绝:
          → symlink-broken                          (priority 5)
      elif resolved_dstage 类型异常或权限拒绝:
          → symlink-broken                          (priority 5)
      else:
          # 跨树独立分类（priority 6-9）
          # v9 per codex v8 P1 #6：target_d_abs / target_dstage_abs 显式定义
          # v11 per codex v10 P1 #4：调用 §八.2 同一 canonicalize 实现
          #   （处理绝对 / 相对 target 分支：`realpath -m` canonicalize；
          #   绝对 target 直接采用， 相对 target 拼接到 symlink_dir_canonical
          #   后再 canonicalize），不再用无条件 `symlink_dir + target` 拼接
          target_d_abs      = canonicalize_d_target(target_d, symlink_d)
          target_dstage_abs = canonicalize_dstage_target(target_dstage, symlink_dstage)

          d_target_in_dstage = is_within(target_d_abs, DSTAGE_SRC_ROOT_CANONICAL)
          dstage_target_in_d  = is_within(target_dstage_abs, D_SRC_ROOT_CANONICAL)

          if d_target_in_dstage:
              → symlink-d2dstage                    (priority 6)
          elif dstage_target_in_d:
              → symlink-dstage2d                    (priority 7)
          elif target_d 字节序 == target_dstage 字节序:
              → symlink-same-target                 (priority 8)
          else:
              → symlink-target-changed              (priority 9)
```

`canonicalize_d_target` 与 `canonicalize_dstage_target` 定义（与
上方 bash 片段 §八.2 一致；v11 删除重复伪代码）：

```python
def canonicalize_d_target(target_d: str, symlink_d: str) -> str:
    """v11 canonicalize for D 端 symlink target：
    - 绝对 target（以 / 开头）：直接采用，再 `realpath -m` canonicalize
      （处理 `..` / `//` / `.`）
    - 相对 target：拼接到 symlink 所在目录的 canonical 路径，再
      `realpath -m` canonicalize
    """
    symlink_d_dir = os.path.dirname(symlink_d)
    symlink_d_dir_canonical = realpath_m(symlink_d_dir)  # 不解析 symlink 自身
    if target_d.startswith("/"):
        target_d_joined = target_d
    else:
        target_d_joined = f"{symlink_d_dir_canonical}/{target_d}"
    return realpath_m(target_d_joined)


def canonicalize_dstage_target(target_dstage: str, symlink_dstage: str) -> str:
    """v11 canonicalize for D_stage 端 symlink target（与 D 端对称）。"""
    symlink_dstage_dir = os.path.dirname(symlink_dstage)
    symlink_dstage_dir_canonical = realpath_m(symlink_dstage_dir)
    if target_dstage.startswith("/"):
        target_dstage_joined = target_dstage
    else:
        target_dstage_joined = f"{symlink_dstage_dir_canonical}/{target_dstage}"
    return realpath_m(target_dstage_joined)
```

**v11 P1 #4 关键变化**：
- 删除旧版 `target_d_abs = realpath -m -- "${symlink_d_dir_canonical}/${target_d}"`
  无条件拼接 — 该写法对绝对 target 错误地将其解释为 symlink 所在目录下的
  路径，违反 §八.2 v10 canonicalize 规范的绝对/相对分支；
- 改为调用 `canonicalize_d_target` / `canonicalize_dstage_target` 同一
  实现（含绝对/相对分支 + `realpath -m` canonicalize），与上方 bash
  片段 §八.2 字节对齐；
- v10 声称覆盖的"含 `..` 组件的绝对 target" fixture（如
  `/tmp/d/../d-stage/x`）现可正确分类为 `symlink-d2dstage`。

**互斥性论证**：

- priority 1-5 单边 / 异常路径，**完全排除** priority 6-9 路径变更分类；
- priority 6-7 跨树分类**完全排除** priority 8-9 同 target / 路径变更分类；
- priority 8 vs 9 由 `target_d == target_dstage` 字节序判定，**完全互斥**。

**v7 变化说明**：

- v6 priority 7 把跨树放在 same-target / target-changed **之后**，导致
  跨树分类在"两端均存在 + target 一致"或"两端均存在 + target 不一致"时
  也可能触发，**不互斥**；
- v7 跨树分类（priority 6-7）放在 same-target / target-changed（priority 8-9）
  **之前**，跨树是更具体的语义（"指向对方子树内"），**优先于**"target
  字节序相等/不等"判定。

### 8.3 SYM-N-A 与"每行追溯 BC/Patch" reconcile（v7 保持 v6 reconcile）

**v7 保持 v6 reconcile 思路**（per codex v6 P1 #2 强化）：

- **symlink 条目不强制追溯 BC/Patch**（v6 reconcile + v7 per codex v6
  P1 #2 强化）：symlink 在 D_stage 完整性审计中的作用是路径变更 /
  跨树引用检测，**不直接关联** P5 BC 矩阵；阶段一 O-2 **不引用**
  P5 435 per-file / 23 BC 矩阵任何 per-file 列表；
- **bc 字段填法**：symlink 条目 `bc` 字段填 `SYM-N-A`（默认）或 `UNASSIGNED`
  （如用户选择不分类）；
- **Patch 追溯**：symlink 条目**不强制**追溯到 19 项台账中的具体 Patch；
  如 Patch 引用，notes 字段可补充"per Patch 026-lifecycle"等参考信息，
  **但不作为闭合判定**；
- **闭合判定 reconcile**：§十四 O-2 闭合判定中，"每行 diff 追溯 BC/Patch"
  的硬约束**仅适用于 regular file 与 directory 条目**；symlink 条目仅需
  满足：9 类分类完整 + 主表与专表一致 + hash 一致 + 不归入 BC。

### 8.4 主表 vs 专表对齐（v7 保持 v6 对齐）

| 项 | 主表（`d-stage-audit.tsv`） | 专表（`d-stage-audit.symlink.tsv`） |
| --- | --- | --- |
| **包含** | 所有 regular file + directory + symlink 条目（6 字段 schema） | 仅 symlink 条目（详 §十二.2 schema） |
| **symlink 字段** | `classification` ∈ `symlink-*`（**9 类之一** v7）+ `bc` = `SYM-N-A` / `UNASSIGNED` | 完整 target + d_rel + d_stage_rel + classification + **9 类细分** |
| **hash 计算** | ✓（必算） | ✓（必算） |
| **重放** | ✓（必重放） | ✓（必重放） |
| **闭合判定** | ✓ | ✓ |
| **追溯 BC** | regular file 必追溯；symlink **不强制** | symlink **不强制** |

### 8.5 symlink 专表 schema（v7 显式定义）

```
classification \t d_rel \t d_stage_rel \t symlink_target_d \t symlink_target_dstage \t bc \t notes
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `classification` | enum | **9 类之一**（v7 per codex v6 P1 #4，详 §八.1） |
| `d_rel` | path | D 端相对路径；single-side-dstage 时为空字符串 |
| `d_stage_rel` | path | D_stage 端相对路径；single-side-d 时为空字符串 |
| `symlink_target_d` | path | D 端 symlink target；D 端不存在时为空字符串 |
| `symlink_target_dstage` | path | D_stage 端 symlink target；D_stage 端不存在时为空字符串 |
| `bc` | enum | `SYM-N-A` / `UNASSIGNED` |
| `notes` | text | 与 P5 交叉核对备注；禁止空（至少填 `OK` 或具体偏差描述） |

**字段数硬约束**：`awk -F'\t' 'NF != 7' < d-stage-audit.symlink.tsv | wc -l`
必须返回 `0`。

### 8.6 排序（v7 显式）

- **主表**：6 字段按 `classification` → `d_rel` → `d_stage_rel` 三级排序
  键 LC_ALL=C 字节序排序（class 顺序：d / f < diff_no 序 ... symlink-*）；
- **专表**：7 字段按 `classification` → `d_rel` → `d_stage_rel` 三级排序
  键 LC_ALL=C 字节序排序（**9 类** class 顺序：same-target /
  target-changed / single-side-d / single-side-dstage / d2dstage /
  dstage2d / dangling / broken / loop，per §八.1 v7）。

## 九、失败关闭

**目的**：任何错误立即停止，不写入部分输出。

**规则**（v24 per codex v7 P1 #1 + P1 #4 + P1 #5 + codex v9 P1 #2+#3 + codex v10 P1 #1+#2+#3 + codex v11 P1 #1+#2 + codex v11 P2 #4 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2 修订；历史 v8 → v9 → v10 → v11 → v12 → v13 → v14 → v15 → v16 → v17 → v18 → v19 → v20 → v21 → v22 → v23 → v24 演化见 §十三 历史对照段）：

1. **原子写**：所有产物（**v24 共 8 稳定文件 + 1 运行时元数据**：8 件套 +
   各自 sha256 + 1 个 genesis.json；详 §十一 v24；v11 per codex v10 P1 #3
   genesis.json 新增 tool_versions.jq + fault_injection_tests 字段；v12 per
   codex v11 P1 #1 扩展 fault_injection_tests 至 7 场景 + per codex v11
   P2 #4 新增 exit_code_test_coverage 字段；v13 per codex v12 P1 #1-#4
   重写 fault_injection_tests 为 8 场景 + 结构化 journal 状态；v14 per
   codex v13 P1 #1+#2 + P2 #3 扩展至 11 场景 + 排他锁 + staging_dir 路径
   约束 + 旧对完整性 fail-closed；v15 per codex v14 P1 #1+#2 锁先于一切
   变更 + 三者事实一致性 + 故障注入 12 场景；v16 per codex v15 P1 #1+#2
   旧对精确指纹 + reconcile 子命令唯一入口 + 故障注入 13 场景；v17 per
   codex v16 P1 #1+#2 + P2 #3 共享分类函数 + 状态专属不变量 + 旧对自洽
   校验 + 故障注入 16 场景；v18 per codex v17 P1 #1+#2 + P2 #3 幂等
   no-op + selfcheck fail-closed + 计数口径修正；v19 per codex v18
   P1 #1+#2 + P2 #3 fsync 屏障（断电一致性）+ bash 草案降级伪代码 +
   Python 契约 + no-op 严格 sidecar 校验 + 故障注入 22 场景（5
   power-loss + 1 no-op 重复行）；v20 per codex v19 P1 #1 + P2 #2
   跨目录 rename 源、目标双目录同步（backed_up ← .txn + OUT_DIR；回滚 ←
   OUT_DIR + .txn）+ 3 个跨目录 rename 断电窗口场景 + 故障注入 25 场景 +
   reconcile 活动规范标签统一 v20；v21 per codex v20 P1 #1+#2+#3
   rolling_back 合法中途态白名单 + 无 journal 恢复走全新路径（废除
   "OUT_DIR 自洽即 exit 0"捷径）+ verified 清理 .txn fsync 屏障（rm
   old.* 后先于 rm journal）+ 3 个新场景 + 故障注入 28 场景）先写入
   `<out-dir>/<file>.tmp`，fsync，再 `rename` 到 `<out-dir>/<file>`；
   rename 失败立即 exit；
   **v19 per codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14
   P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex
   v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 排他锁 +
   持久化事务目录 + 结构化 journal**：
   snapshot 子命令先获取 OUT_DIR 排他锁（fcntl.flock LOCK_EX|LOCK_NB 于
   `.snapshot.lock`，失败 exit 6，锁文件永不删除；**锁在参数解析之后、
   任何文件系统变更之前获取** per codex v14 P1 #1），随后对
   d-stage-snapshot.tar.zst + .sha256 实施固定事务目录 `.txn` + 结构化
   key=value journal 协议（staged / backed_up / tarball_committed /
   sha_committed / verified / rolling_back 状态机 + journal 先于任何移动 +
   文件 fsync + rename + 目录 fsync + **状态依赖数据屏障**（persist(staged)
   ← staging 文件 + staging 目录；persist(backed_up) ← .txn 目录 +
   OUT_DIR 源目录（v20 per codex v19 P1 #1 跨目录 rename 源、目标都同步）；
   persist(tarball_committed/sha_committed) ← OUT_DIR 目录；清理/回滚 ←
   OUT_DIR 目录 + .txn 源目录（回滚方向同样源、目标都同步）；v19 per
   codex v18 P1 #1 断电一致性，顺序不可交换）+
   staging_dir 路径约束（realpath -m +
   is_within(.txn/staging) + 非 symlink + 目录）+ 旧对完整性 fail-closed
   （两者都在或都不在，半对 exit 9）+ 旧对自洽校验（hash 字段 ==
   old_tar_sha256 + sha256sum -c，per codex v16 P2 #3；sidecar 严格 schema
   唯一判定入口，幂等 no-op 复用同一校验 per codex v18 P2 #3）+ 三者事实一致性
   fail-closed（旧对精确指纹 old_tar_sha256 / old_sha_sha256 严格匹配，
   未知/损坏/替换内容 exit 9；共享 classify_out_pair 分类（sidecar 单行
   wc -l==1 + 精确格式 + hash 字段）；计划 present 的旧文件必须恰好存在
   于 .txn 或 OUT_DIR 之一，回滚内容感知去 || true per codex v14 P1 #2 +
   codex v15 P1 #1 + codex v16 P1 #1）+ 状态专属不变量（verified 必须
   完整新对否则 exit 9 不清理证据，per codex v16 P1 #2），详
   §五.3 v24）；v12 的 `.snapshot.journal` + `.old.<pid>` 命名已废弃；
2. **任何工具 exit ≠ 0**：diff / git / python3 / sha256sum / find / jq 任一工具
   exit 非零 → 生成脚本立即 exit 同码（fail-fast），不写入任何产物；
   **v11 per codex v10 P1 #3 新增 jq**：jq 1.7.1 纳入工具链前置；
3. **schema 校验失败（v10 per codex v7 P1 #3 + P1 #4 + codex v10 P1 #2 强化）**：
   - 主表 6 字段校验：`awk -F'\t' 'NF != 6' < d-stage-audit.tsv | wc -l` 必须返回 `0`；
   - 专表 7 字段校验：`awk -F'\t' 'NF != 7' < d-stage-audit.symlink.tsv | wc -l` 必须返回 `0`；
   - **tree-manifest 4 字段校验（v8 P1 #4 修复 brace expansion；v10 保持）**：
     分别对 `d-stage-audit.D.manifest.tsv` 与 `d-stage-audit.D_stage.manifest.tsv`
     各跑一次 `awk -F'\t' 'NF != 4'` 计数（**不再**用 `D{,stage}` brace expansion：
     后者展开为 `D` + `stage` ≠ `D_stage`，per codex v7 P1 #4）；
   - **字段编码校验（v8 P1 #3；v10 保持）**：合并最终 TSV 前逐行 awk 校验
     filename / symlink_target / file_sha256 字段不含 tab/CR/LF/NUL；
   - 任一校验失败 → exit 1（schema violation），不写入；
4. **（v8 per codex v7 P1 #1 删除 F-only exclusion check；v10/v11/v12 保持）**：F-only 检测
   在阶段一 O-2 **不可计算**（不读 F0 + 不持有 F-only 文件清单 → 无法判断
   文件"仅在 F0 存在"）；F-only 检测整体移到阶段三 O-4 闭合后；
5. **symlink 9 类分类完整 + 主表与专表条目数一致**（v7 per codex v6
   P1 #4 修订；v6 称 8 类实为 9 类）：任一缺失 → exit 7（symlink 闭合
   失败）；
6. **SYM-N-A reconcile**：symlink 条目 `bc` 字段仅允许 `SYM-N-A` /
   `UNASSIGNED`；如出现 BC-01..BC-22b → exit 8（SYM-N-A 违反），不写入；
7. **确定性自校验失败**：第一次生成后立即重跑一次（同一脚本 + 同一
   输入），**8 稳定文件** SHA-256 必须匹配（**v8 per codex v7 P1 #6**：
   genesis.json **不计入** SHA 一致判定，仅写入文件本身）；
   不匹配 → exit 2（determinism violation），删产物；
8. **保护区写入检测**：脚本启动时检查 `--out-dir` 是否在保护区清单内；
   在保护区 → exit 3（protected path violation），不写入；
9. **F0 引用检测**：脚本执行期间任何路径前缀匹配 F0 命名空间
   （如 `/fantgpu/`、`/F0/` 等已知模式）+ exit 4（F0 reference violation），
   不写入。

**错误码表（v8 per codex v7 P1 #1 删除 F-only violation；v13 per codex v11 P2 #4 加入作用域列 + codex v12 P1 #1-#4 修订 snapshot 5/9 语义；v24 per codex v23 P1 #1 snapshot 5 扩展含回滚还原 mv 后的目录 fsync 失败）**：

| 退出码 | 含义 | 子命令作用域 |
| --- | --- | --- |
| 0 | 成功（确定性自校验通过 + **8 稳定文件** SHA 一致；genesis.json 不计） | gen-manifest / snapshot / reconcile |
| 1 | schema violation（6/7/4 字段 NF≠预期 / 字段编码） | gen-manifest |
| 1 | staging 内 sha256sum -c 失败 | snapshot |
| 1 | NF≠4 / 字段编码错误 | reconcile |
| 2 | determinism violation（两次 SHA-256 不一致） | gen-manifest |
| 2 | reconcile 失败（4 字段比对不一致） | snapshot / reconcile |
| 3 | protected path violation（输出目录在保护区清单） | gen-manifest / snapshot |
| 3 | tarball / manifest 文件不存在 | reconcile |
| 4 | F0 reference violation（检测到 F0 命名空间引用） | **仅 gen-manifest** |
| 4 | reference manifest 缺失或不可访问（canonicalize 后不存在） | **仅 snapshot** |
| 5 | input SHA-256 mismatch（D / D_stage tree-manifest 与 genesis 不一致） | **仅 gen-manifest** |
| 5 | persist_journal 写入失败或备份 mv 失败或 fsync_file / fsync_dir 失败（含备份 mv 后与回滚还原**每个** mv 后的目录同步失败，v24 per codex v23 P1 #1 统一；journal 保持前一个持久状态，可恢复；旧对不丢失 per codex v12 P1 #3；修复后重跑自动续跑） | **仅 snapshot** |
| 6 | **（v8 per codex v7 P1 #1 删除）**原 F-only violation；F-only 检测整体移到阶段三 O-4（gen-manifest 作用域不再使用 6） | — |
| 6 | OUT_DIR 排他锁获取失败（另一 snapshot 进程持有 .snapshot.lock；per codex v13 P1 #1 新增；调用方稍后重试） | **仅 snapshot** |
| 7 | symlink 闭合失败（v7 per codex v6 P1 #4 修订：**9 类**分类不完整 + 主表与专表条目数不一致） | gen-manifest |
| 8 | SYM-N-A reconcile 失败（v6 新增：symlink 条目 bc 字段含 BC 值） | gen-manifest |
| 9 | journal 不可解析 / state 未知 / journal 字段取值非法 / staging_dir 路径约束失败（非绝对 / 越界 / symlink / 非目录）/ 三者事实一致性失败（旧文件双存在 / 均缺失 / OUT_DIR 未知内容 / 新旧混合）/ .txn 不变式违反（old.* 存在但 journal 缺失）/ staging 新文件指纹不一致 / OUT_DIR 既有快照半对 fail-closed（per codex v12 P1 #1+#2 + codex v13 P1 #2 + P2 #3 + codex v14 P1 #2 + codex v15 P1 #1；要求 dsh 人工清理 .txn + OUT_DIR 半新文件或半对） | **仅 snapshot** |
| 78 | EX_CONFIG（LC_ALL≠C / 工具版本不符） | gen-manifest |
| 78 | EX_CONFIG（LC_ALL≠C / tar/zstd 版本不符 / OUT_DIR 文件系统不保证 atomic rename） | snapshot |
| 其他 | 上游工具 exit code 透传 | 全部 |

**v24 退出码作用域契约（per codex v11 P2 #4 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #2 + codex v15 P1 #1 + codex v16 P1 #1+#2 + codex v17 P1 #1+#2 + codex v18 P1 #1+#2 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2 修订；历史 v12/v14/v15/v16/v17/v18/v19/v20/v21/v22/v23 标签仅保留于变更溯源）**：

1. **错误码仅在子命令作用域内解释**：调用方根据子命令名（gen-manifest
   / snapshot / reconcile）分别解释错误码；严禁跨子命令对照。
2. **测试契约**：O-2 工具实现时必须为每个子命令编写单元测试，
   覆盖该子命令的全部已声明退出码 + 0（成功路径）；测试结果存
   `docs/planning/evidence/d-stage-audit.genesis.json` 的
   `exit_code_test_coverage` 字段（v24 当前 schema 字段；v12 引入）。
3. **共享码 0/2/3 在不同子命令有不同语义**：调用方应先看子命令名，
   再看退出码；不要直接 `if [ $? -eq 2 ]` 假定全局含义。
4. **snapshot 子命令 exit 9 = journal 不可解析 / state 未知 / journal 字段
   取值非法 / staging_dir 路径约束失败 / 三者事实一致性失败（含 OUT_DIR
   未知内容：sha 既非 new_tar_sha256 也非 old_tar_sha256）/ 状态专属
   不变量失败（verified/sha_committed 半对等）/ .txn 不变式违反 /
   staging 指纹不一致 / OUT_DIR 既有半对 fail-closed**（v13 per codex
   v12 P1 #1+#2 重定义 + v14 per codex v13 P1 #2 + P2 #3 扩展 + v15 per
   codex v14 P1 #2 加三者事实一致性 + v16 per codex v15 P1 #1 加未知内容
   与旧对指纹严格匹配 + v17 per codex v16 P1 #1+#2 加共享分类与状态专属
   不变量 + v18 per codex v17 P1 #1+#2 加幂等 no-op 与 selfcheck
   fail-closed + v19 per codex v18 P1 #1+#2 加 fsync 状态依赖数据屏障与
   Python 实现契约（exit 5 语义扩展：fsync_file / fsync_dir 失败同
   persist 失败，journal 保持前一持久状态；v24 per codex v23 P1 #1 回滚段
   逐次还原 mv 后的目录 fsync 失败统一 exit 5））；与全局 exit 表显式区分
   （全局表无 9，snapshot 表独占）；
   要求 dsh 人工清理 `.txn`（含 journal + old.* + staging）+ OUT_DIR
   半新文件或半对后再重跑。
5. **snapshot 子命令 exit 6 = OUT_DIR 排他锁获取失败**（v14 per codex v13
   P1 #1 新增；v15 per codex v14 P1 #1 锁位置移到参数解析之后、任何文件
   系统变更之前）；snapshot 表独占；gen-manifest 作用域不再使用 6（v8 已
   删除原 F-only violation）；调用方收到 6 应稍后重试，不要与 gen-manifest
   的历史 6 混淆。
6. **退出码历史变更记录**：v8 删除 6（gen-manifest F-only）；v11 复用 5；
   v12 新增 9（snapshot 独占）；v13 per codex v12 P1 #3 重定义 snapshot 5
   （= persist_journal 写入失败或备份 mv 失败，可恢复且旧对不丢）；v14
   per codex v13 P1 #1 新增 snapshot 6（锁获取失败）+ per codex v13
   P1 #2 + P2 #3 扩展 snapshot 9 语义；v15 per codex v14 P1 #2 再扩展
   snapshot 9（三者事实一致性失败）；v16 per codex v15 P1 #1 加未知内容
   与旧对指纹；v17 per codex v16 P1 #1+#2 加共享分类与状态专属不变量；
   v18 per codex v17 P1 #1+#2 加幂等 no-op（旧新指纹相等成功返回）与
   selfcheck fail-closed；v19 per codex v18 P1 #1+#2 加 fsync 状态依赖
   数据屏障（exit 5 语义扩展：fsync_file/fsync_dir 失败同 persist 失败）
   与 Python 实现契约；v20 per codex v19 P1 #1 + P2 #2 跨目录 rename 源、
   目标双目录同步（exit 5 语义不变）与 reconcile 活动规范标签统一；
   F-only 检测整体移到阶段三 O-4（不再使用任何退出码）。阶段二 release
   commit 调用 snapshot 时，错误处理代码应按 v24 表解释退出码；旧
   v10/v11/v12/v13/v14/v15/v16/v17/v18/v19/v20/v21/v22/v23 调用方按各自版本表解释（仅
   snapshot 5 语义 v13 起含 journal 写失败，v12 及以前仅为备份 mv 失败；
   snapshot 6 自 v14 起存在；v19 起 exit 5 含 fsync 屏障失败；v24 起
   exit 5 含回滚段逐次还原 mv 后的目录 fsync 失败）。

## 十、确定性重放

**目的**：相同 D + D_stage 输入两次运行产生字节一致的输出。

**不变量**：

1. **排序键固定**（§七）；
2. **LC_ALL=C 强制**（§六）；
3. **工具版本固定**（§六，版本变更视为不可比）；
4. **时间戳确定性**：genesis.json 中的 `started_at` / `finished_at`
   字段**不计入 SHA-256 计算**（仅写入文件本身，避免时间漂移导致
   SHA-256 不一致）；SHA-256 仅覆盖 d-stage-audit.tsv 与
   d-stage-audit.symlink.tsv；
5. **路径规范化**：所有 `d_rel` / `d_stage_rel` 字段去除末尾 `/`，统一用
   `/` 分隔符（不混用 `\`），去除 `./` 前缀。

**自校验流程**：

```
Run 1: tools/d-stage-audit-gen.py --d-root D --d-stage-root D_stage --out-dir OUT
        → d-stage-audit.tsv (sha256 = H1)
Run 2: tools/d-stage-audit-gen.py --d-root D --d-stage-root D_stage --out-dir OUT
        → d-stage-audit.tsv (sha256 = H2)
Assert H1 == H2  # 确定性自校验通过
```

## 十一、输出 hash 规范（v24 共 8 稳定文件 + 各自 sha256 + 1 个运行时 genesis.json；per codex v7 P1 #6 拆分稳定证据 vs 运行时元数据）

**v24 输出 9 文件拆分（per codex v7 P1 #6，v24 复核保持）**：

O-2 v24 沿用 v10 对 v7 的"9 文件"显式拆分，**避免** v7 那种"9 文件 SHA 一致"自相矛盾（genesis 含运行时 + 绝对路径，但同时声明要 SHA 一致）：

- **8 稳定证据文件**（构成"SHA 一致"判定基础；git 长期跟踪 + 第三方独立验证）：
  - 4 个产物 tsv（主表 + 专表 + D manifest + D_stage manifest）
  - 4 个 sha256 锁定文件（每个 tsv 对应一个 .sha256）
- **1 个运行时元数据文件**（**不计入** SHA 一致判定；仅供调试 + 运行时上下文）：
  - `d-stage-audit.genesis.json`（含 `started_at` / `finished_at` / 绝对路径 /
    工具版本 / row_counts 等；不写入任何 sha256 字段，自身也不被 .sha256
    覆盖；详 §十一.3 v8）

| # | 文件 | 类型 | SHA-256 计算 | 写入 .sha256 | 内容 |
| --- | --- | --- | --- | --- | --- |
| 1 | `d-stage-audit.tsv` | 稳定证据 | ✓（必算） | `d-stage-audit.tsv.sha256` | 主表：6 字段 schema（diff_no \t d_rel \t d_stage_rel \t classification \t bc \t notes） |
| 2 | `d-stage-audit.symlink.tsv` | 稳定证据 | ✓（必算） | `d-stage-audit.symlink.tsv.sha256` | 专表：symlink 完整条目（含 **9 类**分类 + target + d_rel + d_stage_rel，per §八.1 v7） |
| 3 | `d-stage-audit.genesis.json` | 运行时元数据 | ✗（不计；含时间戳 + 绝对路径） | — | 生成元数据（详 §十一.3 v8；**不含** f_only_exclusion_check 字段） |
| 4 | `d-stage-audit.D.manifest.tsv` | 稳定证据（**输入 lock 1**） | ✓（必算） | `d-stage-audit.D.manifest.tsv.sha256` | D 源树根 tree-manifest（详 §四 v8） |
| 5 | `d-stage-audit.D_stage.manifest.tsv` | 稳定证据（**输入 lock 2**） | ✓（必算） | `d-stage-audit.D_stage.manifest.tsv.sha256` | D_stage 源树根 tree-manifest（详 §四 v8） |

**v8 per codex v7 P1 #6 关键变化**：

- v7 "9 文件 SHA 一致"自相矛盾（genesis 含运行时 + 绝对路径 → 同一脚本两
  次运行 SHA 必然不同）；v8 显式拆分为 8 稳定 + 1 运行时，"SHA 一致"
  判定**仅**针对 8 稳定文件；
- v7 genesis.json 含 `f_only_exclusion_check` 字段；v8 **删除**该字段
  （F-only 检测整体移到阶段三 O-4 后）；
- v7 主表 schema `d-stage-audit.D{,stage}.manifest.tsv` brace expansion
  路径 bug；v8 改为分别列出两个 manifest 文件。

### 11.1 主表 .sha256 格式

```
# d-stage-audit.tsv.sha256
<sha256>  d-stage-audit.tsv
```

### 11.2 symlink 专表 .sha256 格式

```
# d-stage-audit.symlink.tsv.sha256
<sha256>  d-stage-audit.symlink.tsv
```

### 11.3 genesis.json 字段（v24 per codex v10 P1 #3 + codex v11 P1 #1 + codex v11 P2 #4 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2 + codex v7 P1 #1 删除 f_only_exclusion_check；运行时元数据，**不计入** SHA 一致判定）

```json
{
  "schema_version": "2.4",
  "d_root_path": "<absolute path to D>",
  "d_stage_root_path": "<absolute path to D_stage>",
  "d_manifest_sha256": "<sha256 of d-stage-audit.D.manifest.tsv>",
  "d_stage_manifest_sha256": "<sha256 of d-stage-audit.D_stage.manifest.tsv>",
  "tool_versions": {
    "diffutils": "3.10",
    "git": "2.39.5",
    "python3": "3.11.9",
    "coreutils": "9.4",
    "findutils": "4.9.0",
    "tar": "1.35",
    "zstd": "1.5.7",
    "jq": "1.7.1"
  },
  "snapshot_tool_versions": {
    "tar": "1.35",
    "zstd": "1.5.7",
    "tar_path": "<absolute path to tar used>",
    "zstd_path": "<absolute path to zstd used>",
    "build_environment": "<hostname + uname -a + locale>",
    "notes": "v20 per codex v19 P1 #1 + P2 #2 + codex v18 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v16 P1 #1+#2 + P2 #3 + codex v15 P1 #1+#2 + codex v14 P1 #1+#2 + codex v13 P1 #1+#2 + P2 #3 + codex v12 P1 #1-#4 + codex v11 P1 #1 + codex v10 P1 #3 + codex v9 P2 #6：精确锁定 tar 1.35 + zstd 1.5.7（本机实测）；**v11 新增 jq 1.7.1 工具链前置条件**（阶段三 §9.2 F0 迁移起点的 `git_tag_ref` 读取必须用 jq 或已声明的 JSON 解析入口，不允许临时引入未声明的工具）。如需在非本机环境复现 SHA，必须使用完全相同的 tar + zstd + jq 版本 + 相同构建环境（locale / libc / 容器）。"
  },
  "snapshot_boundary": "v20 per codex v19 P1 #1 + P2 #2 + codex v18 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v16 P1 #1+#2 + P2 #3 + codex v15 P1 #1+#2 + codex v14 P1 #1+#2 + codex v13 P1 #1+#2 + P2 #3 + codex v12 P1 #1-#4 + codex v11 P1 #1 + codex v10 P1 #3 + codex v9 P2 #6：本快照 SHA 仅在 tar 1.35 + zstd 1.5.7 + jq 1.7.1 + 本机构建环境下可复现；其他版本/环境需重新生成 reference snapshot 并按 genesis.json tool_versions 比对。",
  "fault_injection_tests": {
    "schema_version": "13.0",
    "snapshot_subcommand": {
      "lock_contention": {
        "injection": "process A holds .snapshot.lock; start process B concurrently",
        "expected_final_state": "B acquire_snapshot_lock fails → exit 6 + clear error; B exits BEFORE creating any temp file / .txn / staging modification (v15 per codex v14 P1 #1: lock precedes all filesystem changes); A completes normally",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "crash_after_staged_persist": {
        "injection": "kill -9 $$ after persist_journal(state=staged), before backup old tarball mv",
        "expected_final_state": "next run (after re-acquiring lock) parses journal=staged → validates staging_dir path constraints + staging fingerprint (sha256/size) → completes backup mv → backed_up → commit new pair → verified → cleanup; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "crash_between_backup_mvs": {
        "injection": "kill -9 $$ after old tarball mv into .txn, before old sha256 mv",
        "expected_final_state": "journal=staged + facts (old tar in .txn, old sha in OUT_DIR) → recovery completes remaining backup mv (guarded idempotent) → continue; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "crash_after_backed_up_persist": {
        "injection": "kill -9 $$ after persist_journal(state=backed_up), before new tarball mv",
        "expected_final_state": "journal=backed_up → recovery commits new tarball + new sha256 → self-check → cleanup; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "crash_between_tarball_mv_and_journal": {
        "injection": "kill -9 $$ after new tarball mv to OUT_DIR, before persist_journal(state=tarball_committed)",
        "expected_final_state": "journal=backed_up + facts (new tar in OUT_DIR) → recovery skips tarball commit, commits sha256 directly; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "selfcheck_fail": {
        "injection": "write wrong sha256 content (same file size)",
        "expected_final_state": "4e sha256sum -c FAIL → persist rolling_back → rollback segment calls classify_out_pair: tampered file (tarball sha or sidecar hash field no longer matches new fingerprint) → unknown → exit 9 keeping journal and scene (fail-closed; pre-v17 'auto rollback to old pair' expectation retired per codex v17 P1 #2 as it contradicts fail-closed policy; auto-restore path only exists in rolling_back crash re-entry with precisely new/old files)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "out_dir_readonly": {
        "injection": "chmod -w OUT_DIR before snapshot",
        "expected_final_state": "backup mv fails → exit 5; journal stays staged; OUT_DIR = old pair untouched; re-run after fixing perms resumes from staged",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "journal_write_fail_at_staged": {
        "injection": "make filesystem read-only before persist_journal(state=staged)",
        "expected_final_state": "persist fails → exit 5; old pair NEVER moved (ordering guarantee per codex v12 P1 #3); .txn cleaned; no data loss",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "journal_corrupt": {
        "injection": "echo garbage > .txn/journal before snapshot",
        "expected_final_state": "startup parse fails (schema≠1 / state missing) → exit 9 + clear error; dsh manual cleanup of .txn + OUT_DIR partial new files",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "staging_dir_path_traversal": {
        "injection": "manually rewrite journal staging_dir to '.txn/../../etc' or replace .txn/staging with a symlink",
        "expected_final_state": "path constraint validation fails (non-absolute / canonical out-of-range / symlink / not a directory) → exit 9; no mv performed",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "old_pair_incomplete": {
        "injection": "pre-place OUT_DIR with only d-stage-snapshot.tar.zst (no .sha256) before snapshot",
        "expected_final_state": "4a completeness fail-closed → exit 9 + clear error; no journal written, no files moved, OUT_DIR half-pair untouched (dsh manually completes or removes, then re-run)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "old_pair_restore_incomplete": {
        "injection": "delete .txn/old.tar.zst mid-transaction (journal old_tar=present), then re-run",
        "expected_final_state": "three-way facts consistency check fails (plan=present but old tarball absent in both .txn and OUT_DIR) → exit 9 + clear error; new pair NOT deleted, never silently restored to half-pair (dsh manual intervention)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "unknown_content_in_out_dir": {
        "injection": "replace OUT_DIR tarball with a third-party file (sha matches neither new_tar_sha256 nor old_tar_sha256), then re-run",
        "expected_final_state": "fingerprint classification = unknown → exit 9 + clear error; unknown content NEVER misjudged as old file, never deleted, never reported as complete old pair (v16 per codex v15 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "immediate_rollback_sidecar_unknown": {
        "injection": "before in-process rollback, rewrite OUT_DIR .sha256 to a format-legal single line whose hash field != new_tar_sha256",
        "expected_final_state": "shared classify_out_pair classifies sidecar unknown → exit 9; sidecar deleted ONLY when precisely identified as new (single line + exact format + hash field); journal and files preserved (v17 per codex v16 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "verified_half_pair": {
        "injection": "manually craft journal=verified with OUT_DIR containing only the new tarball (no .sha256), then re-run",
        "expected_final_state": "state-specific invariant fails (verified requires both files = new) → exit 9; NO evidence cleanup (journal / old backup / .txn preserved) (v17 per codex v16 P1 #2)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "old_pair_inconsistent": {
        "injection": "pre-place OUT_DIR old pair whose tarball and .sha256 contents mismatch each other (sidecar format-legal), then run snapshot",
        "expected_final_state": "4a self-consistency check fails (hash field != old_tar_sha256 or sha256sum -c fails) → exit 9; inconsistent old pair never enters transaction, corruption evidence not silently discarded (v17 per codex v16 P2 #3)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_staged_persist": {
        "injection": "force power-off (VM poweroff / disk pull; NOT kill -9 — kill -9 keeps page cache and cannot cover power-loss semantics) immediately after persist_journal(state=staged)",
        "expected_final_state": "journal=staged and staging files both durable (v19 fsync barrier per codex v18 P1 #1) → reboot recovery: path constraints + staging fingerprint pass → completes backup mv → backed_up → commit new pair → verified → cleanup; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_before_staged_persist": {
        "injection": "force power-off after staging files written, before persist_journal(state=staged)",
        "expected_final_state": "journal absent (.txn exists without journal and without old.* → ordering invariant holds) → reboot discards .txn residue and continues FRESH PATH (v21 per codex v20 P1 #2: NO LONGER treats a self-consistent OUT_DIR pair as an already-completed new pair and exits 0 — without a journal there is no new-pair fingerprint evidence, and an intact old pair + staging residue would be misjudged; if the existing pair equals the new snapshot, 4a idempotent no-op returns exit 0 under strict validation, otherwise the transaction proceeds normally); old pair untouched; re-run succeeds (v19 per codex v18 P1 #1 + v21 per codex v20 P1 #2)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_backed_up_persist": {
        "injection": "force power-off after persist_journal(state=backed_up)",
        "expected_final_state": "journal=backed_up and .txn/old.* both durable (v19 fsync barrier) → recovery commits new tarball + new sha256 → self-check → cleanup; final OUT_DIR = new pair (v19 per codex v18 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_tarball_mv": {
        "injection": "force power-off after new tarball mv to OUT_DIR and fsync_dir(OUT_DIR), before persist_journal(state=tarball_committed)",
        "expected_final_state": "journal=backed_up + facts (new tar in OUT_DIR with content = new_tar_sha256, directory entry durable) → recovery skips tarball commit and commits sha256 directly; final OUT_DIR = new pair (v19 per codex v18 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_verified_before_cleanup": {
        "injection": "force power-off after persist_journal(state=verified), before rm journal / rm -rf .txn and fsync_dir(OUT_DIR) complete",
        "expected_final_state": "journal=verified (may reappear if directory fsync incomplete) + OUT_DIR complete new pair → state-specific invariant passes → redo cleanup → exit 0; final OUT_DIR = new pair (cleanup idempotent; v19 per codex v18 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "noop_duplicate_sidecar": {
        "injection": "pre-place OUT_DIR old pair whose tarball content == new snapshot content, but .sha256 contains two duplicate format-legal lines (bare sha256sum -c returns 0 for this)",
        "expected_final_state": "idempotent no-op branch reuses strict sidecar validation (exactly one line + exact format + hash field) → validation fails → exit 9; malformed/duplicate sidecar NEVER accepted by no-op, corruption evidence not silently discarded (v19 per codex v18 P2 #3)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_backup_mv_before_persist": {
        "injection": "force power-off after both backup mvs AND fsync_dir(.txn) + fsync_dir(OUT_DIR) complete, before persist_journal(state=backed_up)",
        "expected_final_state": "journal=staged (durable) + facts (old pair only in .txn, OUT_DIR source directory entries deletion durable → NO double-existence) → recovery: backup mv guard skips idempotently → persist(backed_up) → commit new pair → verified → cleanup; final OUT_DIR = new pair (auto-resume; v20 per codex v19 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_backup_mv_fsync_window": {
        "injection": "force power-off after backup mv, before fsync_dir(.txn) + fsync_dir(OUT_DIR) both complete",
        "expected_final_state": "journal=staged; facts three-way: (1) directory entries happen to be consistent → auto-resume per staged facts; (2) OUT_DIR source entry lingers → old file double-existence → exit 9; (3) .txn entry not durable → both absent → exit 9. NEVER silently produces half pair (fsync-incomplete residual window fail-closed; v20 per codex v19 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_after_restore_mv_before_cleanup": {
        "injection": "force power-off after rollback restore mv + fsync_dir(OUT_DIR) + fsync_dir(.txn) complete, before rm journal / rm -rf .txn",
        "expected_final_state": "journal=rolling_back + OUT_DIR complete old pair (target directory entries durable) + .txn has no old.* (source directory deletion durable → NO double-existence) → re-enter rollback segment: no new files to delete, no old.* to restore → self-check PASS → cleanup → exit 1; final OUT_DIR = complete old pair (v20 per codex v19 P1 #1)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_rollback_mid_delete_window": {
        "injection": "force power-off after rollback segment rm of new tarball, before rm of new sidecar",
        "expected_final_state": "journal=rolling_back + OUT_DIR state pair = (absent,new) → rolling_back legal mid-state whitelist (v21 per codex v20 P1 #1) explicitly allows → re-enter rollback segment: continue deleting new sidecar → restore old pair one by one → self-check → cleanup → exit 1; final OUT_DIR = complete old pair (NEVER exit 9)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_rollback_mid_restore_window": {
        "injection": "force power-off after rollback segment mv of old tarball back to OUT_DIR (with its per-mv fsync_dir(OUT_DIR) + fsync_dir(.txn) complete), before mv of old sidecar",
        "expected_final_state": "journal=rolling_back + OUT_DIR state pair = (old,absent) (DETERMINISTIC: v23 per codex v22 P1 #1 per-mv restore barrier guarantees the first mv's source and target directory entries are durable → NO double-existence / both-missing) → rolling_back legal mid-state whitelist explicitly allows → re-enter rollback segment: no new files to delete, complete old sidecar restore (with its own per-mv fsync) → self-check → cleanup → exit 1; final OUT_DIR = complete old pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_rollback_restore_fsync_window": {
        "injection": "force power-off after rollback segment restore mv (of old tarball or old sidecar) completes, before that mv's fsync_dir(OUT_DIR) + fsync_dir(.txn) both complete",
        "expected_final_state": "journal=rolling_back; per-restored-file four-way facts matrix (v24 per codex v23 P2 #2, same contract as backup-direction mv-fsync window): old-tar restore mv — (1) target-only (old tar only in OUT_DIR) → whitelist (old,absent); (2) source-only (old tar only in .txn) → (absent,absent) with .txn holding old.*; old-sidecar restore mv (old tar already durably restored → OUT tar = old) — (3) target-only (old sidecar only in OUT_DIR) → whitelist (old,old); (4) source-only (old sidecar only in .txn) → (old,absent) with .txn holding old.*; consistent facts (1)-(4) explicitly allowed by the rolling_back legal mid-state whitelist → re-enter rollback segment idempotently → converge; old file double-existence (present in both OUT_DIR and .txn) → three-way facts check exit 9 (journal + scene preserved); both absent → three-way facts check exit 9 (evidence preserved; NEVER silently produces half pair / empty pair, and NEVER accepts sidecar loss merely because the OUT state pair happens to be a legal mid-state, fail-closed)",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      },
      "power_loss_verified_cleanup_after_journal_rm": {
        "injection": "force power-off after 4e/verified cleanup rm old.* + fsync_dir(.txn) + rm journal, before rm -rf .txn",
        "expected_final_state": "journal absent + .txn exists without old.* (v21 per codex v20 P1 #3: old.* deletion precedes journal deletion and was fsynced) → discard .txn residue and continue fresh path (v21 per codex v20 P1 #2) → existing new pair equals new snapshot → 4a idempotent no-op returns exit 0 under strict validation; final OUT_DIR = new pair",
        "actual_final_state": "<filled by O-2 tool>",
        "passed": <bool>
      }
    },
    "notes": "v24 per codex v23 P1 #1+P2 #2 + codex v22 P1 #1 + codex v21 P1 #1 + codex v20 P1 #1+#2+#3 + codex v19 P1 #1 + P2 #2 + codex v18 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v16 P1 #1+#2 + P2 #3 + codex v15 P1 #1+#2 + codex v14 P1 #1+#2 + codex v13 P1 #1+#2 + P2 #3：共 29 场景（v14=11 / v15=12 / v16=13 / v17=16 / v18=16 / v19=22 / v20=25 / v21=28 / v22=28 / v23=28 / v24=29；v24 新增 1 个场景：回滚还原 mv 与逐次双目录 fsync 之间断电窗口 per codex v23 P2 #2——还原 mv 后、任一目录 fsync 未完成时断电 → 三态契约（一致事实可重入 / 旧文件双存在 / 均缺失 → exit 9 保留证据，fail-closed），并统一回滚段逐次 fsync 失败退出码为 exit 5 per codex v23 P1 #1；v23 无新增场景，仅修订回滚还原逐次 mv 屏障与 power_loss_rollback_mid_restore_window 期望确定性 per codex v22 P1 #1；v22 无新增场景，仅修订 rolling_back 白名单可执行等价判定 per codex v21 P1 #1——shell case 未转义 | 是模式分隔符，白名单判定必须用 tuple/set 成员判断（rolling_back_mid_state_ok）或带引号精确字符串匹配；v21 新增 3 个场景：rolling_back 中途删除窗口 / rolling_back 中途还原窗口 / verified 清理 journal 删除后 .txn 残留窗口，v20 新增 3 个跨目录 rename 断电窗口场景，v19 新增 5 个 power-loss 场景 + 1 个 no-op 重复行 sidecar 场景）。power-loss 场景注入方式 = 强制断电（VM poweroff / 拔盘后重启），**不得用 kill -9 代替**（kill -9 不丢 page cache，无法覆盖断电语义）；测试环境要求 OUT_DIR 所在文件系统为本地崩溃一致性文件系统（ext4/xfs 默认 barrier 语义），NFS/9P/virtiofs 已由 exit 78 排除。跨目录 rename（备份 OUT_DIR→.txn、回滚 .txn→OUT_DIR）必须同步源、目标两个目录（v20 per codex v19 P1 #1）；回滚还原**每个** mv 后立即同步源、目标目录（v23 per codex v22 P1 #1 逐次 mv 屏障；v24 per codex v23 P1 #1 任一 fsync 失败 exit 5 统一退出码契约）；回滚还原 mv 后、任一目录 fsync 未完成时断电 → 按**被还原文件**分列四态矩阵（旧 tar 的还原 mv：target-only → 白名单 (old,absent) / source-only → (absent,absent) 且 .txn 有 old.*；旧 sidecar 的还原 mv：target-only → 白名单 (old,old) / source-only → (old,absent) 且 .txn 有 old.*；一致事实均白名单显式允许 → 重入收敛）+ 旧文件双存在 / 均缺失 → 三者事实校验 exit 9 保留证据（**不得仅凭 OUT 状态对恰为合法中途态而错误接受 sidecar 丢失**，fail-closed）（v24 per codex v23 P2 #2）；rolling_back 合法中途态白名单 (new,new)/(absent,new)/(absent,absent)/(old,absent)/(old,old)（v21 per codex v20 P1 #1；可执行等价判定 v22 per codex v21 P1 #1）；无 journal 恢复一律继续全新路径、不因 OUT_DIR 自洽直接 exit 0（v21 per codex v20 P1 #2）；verified 清理 rm old.* 后先 fsync_dir(.txn) 再删 journal（v21 per codex v20 P1 #3）。故障注入测试**必须**在 O-2 工具实现时一并实现，未经测试通过的 snapshot 子命令禁止在阶段二 release commit 中使用。"
  },
  "exit_code_test_coverage": {
    "schema_version": "1.0",
    "gen-manifest": ["0", "1", "2", "3", "4", "5", "7", "8", "78"],
    "snapshot": ["0", "1", "2", "3", "4", "5", "6", "9", "78"],
    "reconcile": ["0", "1", "2", "3"],
    "notes": "v19 per codex v11 P2 #4 + codex v13 P1 #1 + codex v14 P1 #2 + codex v15 P1 #1 + codex v16 P1 #1+#2 + codex v17 P1 #1+#2 + codex v18 P1 #1+#2：每个子命令的退出码作用域契约必须由单元测试覆盖（每个已声明退出码 + 0 成功路径）；退出码仅在子命令作用域内解释，严禁跨子命令对照；snapshot 6 = 排他锁获取失败（v14 新增，v15 锁位置前移）；snapshot 9 含三者事实一致性失败、OUT_DIR 未知内容与状态专属不变量失败（v15/v16/v17/v18 扩展）；snapshot 5 v19 起含 fsync 屏障失败（v15-v19 扩展）。"
  },
  "lc_all": "C",
  "started_at": "<ISO 8601>",
  "finished_at": "<ISO 8601>",
  "row_counts": {
    "differs": <N>,
    "identical": <N>,
    "d_only": <N>,
    "d_stage_only": <N>,
    "symlink_total": <N>,
    "symlink_same_target": <N>,
    "symlink_target_changed": <N>,
    "symlink_single_side_d": <N>,
    "symlink_single_side_dstage": <N>,
    "symlink_d2dstage": <N>,
    "symlink_dstage2d": <N>,
    "symlink_dangling": <N>,
    "symlink_broken": <N>,
    "symlink_loop": <N>
  },
  "byte_counts": {
    "d-stage-audit.tsv": <N>,
    "d-stage-audit.symlink.tsv": <N>,
    "d-stage-audit.D.manifest.tsv": <N>,
    "d-stage-audit.D_stage.manifest.tsv": <N>
  },
  "f0_reference_check": "pass (no F0 paths detected)",
  "determinism_self_check": "pass",
  "schema_check": "pass",
  "symlink_bc_check": "pass (symlink entries use SYM-N-A or UNASSIGNED only)"
}
```

**任何复核者**：执行 `sha256sum docs/planning/evidence/d-stage-audit.tsv` /
`d-stage-audit.symlink.tsv` / `d-stage-audit.D.manifest.tsv` /
`d-stage-audit.D_stage.manifest.tsv` 后比对对应 `.sha256` 文件即可验证，
无需跟踪 `build/` 输出。

## 十二、6 字段 schema（每行 6 字段，固定）

```
diff_no \t d_rel \t d_stage_rel \t classification \t bc \t notes
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `diff_no` | int | 自增序号（1, 2, ..., N），N 由实际生成决定 |
| `d_rel` | path | D 端相对路径；D-only / identical 时可为空字符串 |
| `d_stage_rel` | path | D_stage 端相对路径；D-only 时可为空字符串 |
| `classification` | enum | `differs` / `identical` / `D-only` / `D_stage-only` / **9 类 symlink 细分**（详 §八.1） |
| `bc` | enum | regular file/dir 时填 `BC-01..BC-21` / `BC-22a` / `BC-22b` / `UNASSIGNED`；symlink 时填 `SYM-N-A` / `UNASSIGNED`（**不归入 P5 23 BC 矩阵**，详 §八.3 reconcile） |
| `notes` | text | 与 P5 交叉核对备注；禁止空（至少填 `OK` 或具体偏差描述） |

**字段数硬约束**：`awk -F'\t' 'NF != 6' < d-stage-audit.tsv | wc -l` 必须返回 `0`。
schema 校验失败按 §九 失败关闭规则 exit 1。

**`d-stage-audit.symlink.tsv` 7 字段 schema**：详 §八.5（仅 symlink 条目
7 字段；`awk -F'\t' 'NF != 7' < d-stage-audit.symlink.tsv | wc -l` 必须返回 `0`）。

## 十三、阶段一 O-2 闭合判据（v24 per codex v7 P1 #1 删除 F-only 通用规则；**不引用 P5 任何字段**）

**历史对照（仅作 v6/v7/v8/v9/v10/v11/v12/v13/v14/v15/v16/v17/v18/v19/v20/v21/v22/v23 演化记录，当前不引用）**：

- **v8 关键变化（per codex v7 P1 #1）**：v7 §十三 包含"不硬编码 F-only 文件名清单：F-only 检查改为通用规则"项，要求 F-only 检测在 O-2 阶段执行；这与 O-2 不读 F0 + 不持有 F-only 文件清单的前提**矛盾**（per codex v7 P1 #1）。v8 删除该项，F-only 检测整体移到阶段三 O-4 闭合后。

- **v7 关键变化（per codex v6 P1 #2）**：v6 §十三 标题为"与 P5 BC 矩阵的交叉核对"，要求 O-2 与 P5 23 BC 矩阵 per-file 列表交叉核对、追溯、补登记；这意味着阶段一 O-2 **间接依赖 F0 比较矩阵**（P5 23 BC 矩阵包含 F0 决策信息）。v7 per codex v6 P1 #2 修订：阶段一 O-2 **仅做 D → D_stage 完整性闭合**，**不引用 P5 任何 per-file 列表 / 23 BC 矩阵 / 435 证据**；F0 比较决策全部移到阶段三裁决阶段（O-4 闭合后）。

- **v9 关键变化（per codex v8 6 P1 + 2 P2）**：v9 在 v8 基础上叠加 reconcile 4 字段完整校验 + snapshot hash 固定最终 basename + manifest+sha256 原子提交 + 9 类 symlink `target_*_abs` + `is_within` 显式定义 + tar.zst 精确版本锁定。v9 O-2 闭合判据 16 项（§十四）的 1-4 项未闭合条件与 v8 一致。

- **v10 关键变化（per codex v9 5 P1 + 3 P2）**：v10 在 v9 基础上叠加 reconcile 全局按 (file_type, relative_path) 排序 + 目录级 staging + reconcile-first + 9 类 symlink `realpath -m` canonicalize + 6 fixture + 三子命令接口。v10 O-2 闭合判据沿用 v9 的 16 项结构（仅规范修订，不改变判据条目）。

- **v11 关键变化（per codex v10 4 P1 + 1 P2）**：v11 在 v10 基础上叠加备份-回滚协议 + 故障注入测试要求 + `--reference-manifest` 统一 CLI + jq 工具链前置 + meta.json 路径统一到 evidence dir + 全判定流程引用同一 canonicalize 实现。v11 O-2 闭合判据沿用 v10 的 16 项结构（仅规范修订）。

- **v12 关键变化（per codex v11 2 P1 + 2 P2）**：v12 在 v11 基础上叠加 备份-回滚-journal 持久化 + 启动恢复机制（废弃 v11 的 trap 回滚 + `.bak.$$` 命名）/ `--reference-manifest` 实际 CLI 解析（删除"必须显式传递"措辞，两种形式 `--reference-manifest=<path>` 与 `--reference-manifest <path>` 均支持）/ 退出码子命令作用域契约 + journal 损坏退出码 9 + genesis.json `fault_injection_tests` 7 场景 + `exit_code_test_coverage` 字段 / v10 当前口径统一为 v11。v12 O-2 闭合判据沿用 v11 的 16 项结构（仅规范修订）。

- **v13 关键变化（per codex v12 4 P1 + 1 P2）**：v13 在 v12 基础上叠加 固定事务目录 `.txn`（无 PID；journal 记录 staging_dir + 新文件 sha256/size 指纹，恢复时校验并重新绑定）/ 结构化 key=value 多行 journal 逐字段解析（废弃 v12 的"整行裸比较"）/ journal 持久化先于任何旧文件移动（废弃 v12 "journal 写失败 → 旧对丢失"场景）/ python3 显式 fsync 文件 + rename + fsync 目录路径（禁止全局 sync）/ 故障注入 8 场景 + genesis schema 1.3 / mapping-table + audit 当前状态标签统一 v12（per codex v12 P2 #5）。v13 O-2 闭合判据沿用 v12 的 16 项结构（仅规范修订）。

- **v14 关键变化（per codex v13 2 P1 + 2 P2）**：v14 在 v13 基础上叠加 OUT_DIR 排他锁（fcntl.flock LOCK_EX|LOCK_NB 于 `.snapshot.lock`，失败 exit 6，锁文件永不删除）/ staging_dir 路径约束（realpath -m + is_within(.txn/staging) + 非 symlink + 目录）+ journal 字段严格校验（old_tar/old_sha ∈ {present,absent} 且一致 + new_tar_sha256 64 hex + new_tar_size 数字）/ 旧对完整性 fail-closed（两者都在或都不在，半对 exit 9）/ 故障注入 10 场景 + genesis schema 1.4 / 退出码作用域契约当前标签统一 v14（per codex v13 P2 #4）。v14 O-2 闭合判据沿用 v13 的 16 项结构（仅规范修订）。

- **v15 关键变化（per codex v14 2 P1 + 2 P2）**：v15 在 v14 基础上叠加 排他锁位置前移（参数解析之后、任何文件系统变更之前，per codex v14 P1 #1）/ journal 计划 + .txn/old.* + OUT_DIR 三者事实一致性 fail-closed 校验（内容感知区分新旧；回滚去 || true，mv 失败 exit 9 + journal 重入收敛，per codex v14 P1 #2）/ 活动接口 v12/v13 标签统一 v15（per codex v14 P2 #3）/ 三类命名参数统一真实解析 + 未知参数 fail-closed（per codex v14 P2 #4）/ 故障注入 11 场景 + genesis schema 1.5。v15 O-2 闭合判据沿用 v14 的 16 项结构（仅规范修订）。

- **v16 关键变化（per codex v15 2 P1 + 1 P2）**：v16 在 v15 基础上叠加 journal 新增 old_tar_sha256 / old_sha_sha256 旧对精确指纹，恢复时严格匹配（OUT_DIR 分类 absent/new/old/unknown，未知/损坏/替换内容一律 exit 9；.sha256 精确格式 + hash 字段校验，per codex v15 P1 #1）/ reconcile 唯一合法入口统一为 `tools/d-stage-audit-gen.py reconcile`，旧 v11 bash 独立脚本降级为历史对照段（per codex v15 P1 #2）/ 活动规范 v8/v10/v12 标签残留统一 v16（per codex v15 P2 #3）/ 故障注入 12 场景 + genesis schema 1.6。v16 O-2 闭合判据沿用 v15 的 16 项结构（仅规范修订）。

- **v17 关键变化（per codex v16 2 P1 + 1 P2）**：v17 在 v16 基础上叠加 共享 classify_out_pair() 分类函数（即时回滚与启动恢复共用；sidecar 单行 wc -l==1 + 精确格式 + hash 字段，仅精确 new 可删除，per codex v16 P1 #1）/ 状态专属不变量（verified/sha_committed 必须完整新对、staged 不得 new、backed_up sha 必须 absent、tarball_committed tar 必须 new；违反 exit 9 不清理证据，per codex v16 P1 #2）/ 4a 旧对自洽校验（hash 字段 == old_tar_sha256 + sha256sum -c，失败 exit 9，损坏证据不被静默丢弃，per codex v16 P2 #3）/ 故障注入 16 场景（v18 per codex v17 P2 #3 计数口径修正，v17 文本当时误标 14）+ genesis schema 1.7。v17 O-2 闭合判据沿用 v16 的 16 项结构（仅规范修订）。

- **v18 关键变化（per codex v17 2 P1 + 2 P2）**：v18 在 v17 基础上叠加 4a 幂等 no-op（既有快照与新快照指纹相等且自洽 → 清理 staging 成功返回 exit 0，消除 classify 优先标 new 的旧新指纹相等歧义，per codex v17 P1 #1）/ selfcheck_fail 期望统一 fail-closed（篡改 → unknown → exit 9 保留 journal 与现场，删除旧的"自动回滚到旧对"预期，per codex v17 P1 #2）/ 故障注入计数口径修正为 16 场景（v14=11/v15=12/v16=13/v17=16，per codex v17 P2 #3）/ 活动规范 v16/v15 标签残留统一 v18（per codex v17 P2 #4）+ genesis schema 1.8。v18 O-2 闭合判据沿用 v17 的 16 项结构（仅规范修订）。

- **v19 关键变化（per codex v18 2 P1 + 1 P2）**：v19 在 v18 基础上叠加 fsync 状态依赖数据屏障（persist(staged) ← staging 文件+目录 / persist(backed_up) ← .txn 目录 / persist(tarball_committed/sha_committed) ← OUT_DIR 目录 / 清理与回滚 ← OUT_DIR 目录，顺序不可交换，任一失败 exit 5；断电后 state=staged 必搭配可用 staging 文件，per codex v18 P1 #1）/ 5 个 power-loss 故障注入场景（强制断电，不得用 kill -9 代替，per codex v18 P1 #1）/ design §5.3 bash block 降级为算法伪代码草案 + §五 Python 实现契约（唯一实现依据，per codex v18 P1 #2）/ 幂等 no-op 复用 4a 严格 sidecar 校验（恰好一行 + 精确格式 + hash 字段，裸 sha256sum -c 弃用）+ noop_duplicate_sidecar 故障场景（per codex v18 P2 #3）/ 故障注入 22 场景（v19=22：5 power-loss + 1 no-op 重复行）+ genesis schema 1.9。v19 O-2 闭合判据沿用 v18 的 16 项结构（仅规范修订）。

- **v20 关键变化（per codex v19 1 P1 + 1 P2）**：v20 在 v19 基础上叠加 跨目录 rename 源、目标双目录同步（备份 OUT_DIR→.txn 后 fsync_dir(.txn) + fsync_dir(OUT_DIR)；回滚 .txn→OUT_DIR 后 fsync_dir(OUT_DIR) + fsync_dir(.txn)；只同步目标目录会在断电后残留源目录项 → 旧文件双存在 → 恢复 exit 9，与 backed_up power-loss 自动续跑矛盾，per codex v19 P1 #1）/ 3 个跨目录 rename 断电窗口故障场景（备份 mv 后 persist 前 / 备份 mv 与双目录 fsync 之间 / 回滚还原 mv 后清理前，per codex v19 P1 #1）/ reconcile 活动规范标签 v16 → v20（历史归属保留于括号溯源 per codex v15 P1 #2，per codex v19 P2 #2）/ 故障注入 25 场景（v20=25：3 跨目录 rename 断电窗口）+ genesis schema 2.0。v20 O-2 闭合判据沿用 v19 的 16 项结构（仅规范修订）。

- **v21 关键变化（per codex v20 3 P1）**：v21 在 v20 基础上叠加 rolling_back 合法中途态白名单（统一混合判定仅对非 rolling_back 状态生效；逐文件删除/还原的中间事实态 (new,new)/(absent,new)/(absent,absent)/(old,absent)/(old,old) 显式允许并落入回滚段幂等重入，其余组合 exit 9 保留证据，per codex v20 P1 #1）/ 2 个回滚中途窗口故障场景（回滚中途删除窗口 / 回滚中途还原窗口，per codex v20 P1 #1）/ 无 journal 恢复废除"OUT_DIR 自洽即 exit 0"捷径、统一丢弃 .txn 残留后继续全新路径由 4a 幂等 no-op 严格收敛 + 1 个 journal 删除后 .txn 残留窗口场景（per codex v20 P1 #2）/ verified 清理顺序 rm old.* → fsync_dir(.txn) → rm journal → rm -rf .txn → fsync_dir(OUT_DIR)（4e 与恢复 verified 分支同步；防"journal 删除持久但 old.* 未持久"→ 无 journal 分支 exit 9，per codex v20 P1 #3）/ 故障注入 28 场景（v21=28：2 回滚中途窗口 + 1 journal 删除后残留窗口）+ genesis schema 2.1。v21 O-2 闭合判据沿用 v20 的 16 项结构（仅规范修订）。

- **v22 关键变化（per codex v21 1 P1）**：v22 在 v21 基础上叠加 rolling_back 白名单可执行等价判定（v21 的 shell case 表达式未转义 |——| 是模式分隔符而非状态对字符串的一部分，五个白名单状态对实际全部落入拒绝分支 exit 9，v21 新增的两个回滚中途窗口场景无法恢复；v22 改为带引号精确匹配 `"new|new"|"absent|new"|"absent|absent"|"old|absent"|"old|old"` + §五 Python 契约新增 rolling_back_mid_state_ok（tuple/set 成员判断，禁止照抄 shell case 未转义 | 语法），per codex v21 P1 #1）/ 故障注入 28 场景不变（场景集无变化）+ genesis schema 2.2。v22 O-2 闭合判据沿用 v21 的 16 项结构（仅规范修订）。

- **v23 关键变化（per codex v22 1 P1）**：v23 在 v22 基础上叠加 回滚还原逐次 mv 屏障（两处回滚段——恢复 rolling_back 分支与 4e 即时回滚段——此前在两次还原 mv 全部完成后才统一 fsync 源、目标目录，第一次 mv 后、第二次 mv 前断电时 (old,absent) 无持久化保证（旧 tar 双存在或均缺失 → 三者事实校验 exit 9）；v23 改为**每个**跨目录还原 mv 后立即 fsync_dir(OUT_DIR) + fsync_dir(.txn)，power_loss_rollback_mid_restore_window 期望改为确定性 (old,absent)，per codex v22 P1 #1）/ 故障注入 28 场景不变（场景集无变化）+ genesis schema 2.3。v23 O-2 闭合判据沿用 v22 的 16 项结构（仅规范修订）。

- **v24 关键变化（per codex v23 1 P1 + 1 P2）**：v24 在 v23 基础上叠加 回滚 fsync 退出码契约统一（两处回滚段伪代码将逐次还原后的 fsync_dir 失败定义为 exit 9，与 §五 Python 契约"所有 fsync_file/fsync_dir 失败均 exit 5"冲突 → 统一为 exit 5，journal 保持 rolling_back 重入收敛，per codex v23 P1 #1；伪代码 / 错误码表 snapshot 5 / 退出码测试说明同步）+ 新增 power_loss_rollback_restore_fsync_window 故障场景（回滚还原 mv 后、任一目录 fsync 未完成时断电 → 三态契约：一致事实可重入 / 旧文件双存在 / 均缺失 → 三者事实校验 exit 9 保留证据，fail-closed，与备份方向 mv-fsync 窗口同一契约，per codex v23 P2 #2）/ 故障注入 29 场景（+1）+ genesis schema 2.4 + fault schema 13.0。v24 O-2 闭合判据沿用 v23 的 16 项结构（仅规范修订）。

**阶段一 O-2 闭合判据（v24）**：

1. **D_stage 物化根就位**（`/tmp/r16-d-stage/`，per §〇.5 v7）：
   - D_stage 源树已生成；
   - D_stage 源树 SHA-256 锁定（`d-stage-audit.D_stage.manifest.tsv.sha256`）；
2. **D 源树根就位**（`migration/supervised-source-tree/` 内 dsh 指定子路径）：
   - D 源树 SHA-256 锁定（`d-stage-audit.D.manifest.tsv.sha256`）；
3. **完整性闭合**：D_stage 中所有**实际存在**文件均能由 D + 阶段一中性
   变更记录声明的有效 patch 清单 + 应用顺序**重放得到**（无 hidden
   change / 无 missing patch）；
4. **D_stage-only 文件**（D 中不存在但 D_stage 中存在）**显式标注原因**
   （新增 patch / 路径重命名 / 其他非 Deepin 来源）；
5. **D-only 文件**（D 中存在但 D_stage 中不存在）**显式标注原因**
   （删除 / 重命名 / 其他非 Deepin 来源）；
6. **不引用 P5**：阶段一 O-2 **不引用** P5 任何 per-file 列表 / 23 BC
   矩阵 / 435 per-file 证据；**不交叉核对** P5；**不补登记** P5
   任何条目；**所有 F0 比较决策**移到阶段三（O-4 闭合后 + 用户阶段
   三批准）；
7. **（v8 per codex v7 P1 #1 删除）**原"F-only 检查通用规则"项已删除；
   F-only 检测整体移到阶段三 O-4 闭合后；
8. **symlink 9 类互斥**（per §八.1 v7）：主表与专表条目数一致 + 9 类分类
   完整 + 主表与专表条目数一致作为闭合判定（**不**追溯 BC/Patch，
   per §八.3 reconcile）。

**v8 §十三 vs v7 §十三 关键差异**：

- **v7**：要求 F-only 检测通用规则（→ 在不读 F0 前提下不可计算）；
- **v8**：F-only 检测整体移到阶段三 O-4 后（→ O-2 不再尝试不可计算的检测）；
- **v7**：F-only exclusion_check 计入 §十四 O-2 闭合判定；
- **v8**：§十四 O-2 闭合判定删除 F-only exclude check 项。

## 十四、O-2 闭合判定（v24 per codex v7 P1 #1 + P1 #6；何时算"完成"）

O-2 视为**全闭合**（阶段一即可进入终审）**当且仅当**：

1. O-1 全闭合（19 项台账全部展开为 Deepin 中性变更记录，详 O-1 v7 §五）；
2. D_stage 源树 tree-manifest 已锁定（`d-stage-audit.D_stage.manifest.tsv.sha256` 已写入）；
3. D 源树根 tree-manifest 已锁定（`d-stage-audit.D.manifest.tsv.sha256` 已写入）；
4. **生成脚本 `tools/d-stage-audit-gen.py` 已实现**（v24 仍为占位；实现
   唯一依据 = §五 v24 Python 契约，design §5.3 bash 草案仅为算法伪代码），通过
   codex 阶段一复审 + dsh 终审；
5. 实际生成的 D → D_stage diff 行数 N + **9 类** symlink 各类行数已确定；
6. 全部主表 6 字段 schema 校验通过（NF=6 锁定）；
7. 全部专表 7 字段 schema 校验通过（NF=7 锁定）；
8. **symlink 9 类分类完整 + 主表与专表条目数一致**（per §八.4 对齐 + §八.1 v7）；
9. **SYM-N-A reconcile 通过**（symlink 条目 `bc` 字段仅含 `SYM-N-A` / `UNASSIGNED`，
   **无** BC-01..BC-22b 值；per §八.3 reconcile）；
10. **（v8 per codex v7 P1 #1 删除）**原"F-only exclude check 通过"项已删除
    （F-only 检测不可计算 + 整体移到阶段三 O-4 后，详 §十三 v8）；
11. **不引用 P5 任何 per-file 列表 / 23 BC 矩阵**（per §十三 v8）；
12. **D_stage-only / D-only 文件全部显式标注原因**（per §十三 v8 项 4-5）；
13. **f0_reference_check = pass**（genesis.json 中显式声明未引用 F0 路径；
    per §九 退出码 4 检测）；
14. 确定性自校验通过（两次运行所有 **8 稳定文件** SHA-256 一致；
    genesis.json **不计入**，per codex v7 P1 #6）；
15. 输出物**9 文件**（8 稳定 + 1 运行时）已写入 `docs/planning/evidence/`
    （v7 per codex v6 P1 #5；v8 per codex v7 P1 #6 拆分稳定/运行时）：
    - `d-stage-audit.tsv` + `d-stage-audit.tsv.sha256`（稳定证据）
    - `d-stage-audit.symlink.tsv` + `d-stage-audit.symlink.tsv.sha256`（稳定证据）
    - `d-stage-audit.D.manifest.tsv` + `d-stage-audit.D.manifest.tsv.sha256`（稳定证据，输入 lock 1）
    - `d-stage-audit.D_stage.manifest.tsv` + `d-stage-audit.D_stage.manifest.tsv.sha256`（稳定证据，输入 lock 2）
    - `d-stage-audit.genesis.json`（运行时元数据，不计 SHA 一致判定）
16. 通过 codex 阶段一复跑与 finding 闭环。

**当前状态（v24）**：第 1-4 项未闭合（O-1 未全闭合、tree-manifest 未生成、
脚本未实现）；第 5-16 项全部依赖第 1-4 项。**O-2 v24 远未全闭合**，O-1
全闭合 + 脚本实现 + tree-manifest 生成后才能推进（v24 在 v23 基础上叠加：
回滚段逐次 fsync 失败退出码统一 exit 5（per codex v23 P1 #1）+
power_loss_rollback_restore_fsync_window 三态契约场景 + 故障注入 29
场景（+1）+ genesis schema 2.4 + fault schema 13.0；这些规范的实现仍待
O-1 全闭合 + 用户阶段一批准后启动）。

## 十五、不在 O-2 范围内

- 030-NNN 内容设计（属 O-1 / 框架 §五 / 阶段三产物）；
- 任何 F0 比较决策（属阶段三，**禁止**在 O-2 中涉及；per §十三 v8
  阶段一 O-2 **不引用** P5 任何 per-file 列表 / 23 BC 矩阵 / 435 per-file 证据）；
- 许可证审查结论（属框架 §六.5 / 阶段三产物）；
- 修改任何保护区路径（drivers/、baselines/、debs/、vendor/、build/、
  third_party/、migration/supervised-source-tree/ **任何子路径**
  （v7 per codex v6 P1 #1 强化）、binary-manifest.json、patches/ 顶层
  （除 030-NNN 阶段三允许项外，per 框架 §〇.6 + §十一 v7））；
- 任何试图预填 d-stage-audit.tsv 内容的草稿行为（必须工具生成）；
- 任何写入 `build/` 的 O-2 产物（v5 强制：仅 `docs/planning/evidence/`）；
- 任何引用 F0 路径 / 内容 / 命名的脚本逻辑（v7 §九 退出码 4）；
- 阶段二 `4.0.2-i3` 发布相关产物（属阶段二产物，不在 O-2 范围）；
- **（v8 per codex v7 P1 #1 删除）**原"F-only 文件分类"通用规则项已删除
  （F-only 检测整体移到阶段三 O-4 闭合后，O-2 不再尝试）；F-only 文件
  名清单仅在阶段三 O-4 闭合后由 dsh 提供，**不**再写入 genesis.json
  `f_only_exclusion_check` 字段。

## 十六、配套记录（v24）

- `docs/planning/030-patch-rederivation-design.md`（**v24** 框架本体，
  三阶段方案 + 分阶段门槛 + D_stage `/tmp/r16-d-stage/` + patches/ 三层
  规则 + §〇.1 三列对齐 + §5.3 v24 tar.zst 可复现规范 + §5.3 v24 排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal 协议（含旧对精确指纹）+ 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 幂等 no-op（复用严格 sidecar 校验）+ selfcheck fail-closed + fsync 状态依赖数据屏障（跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行等价判定）+ 无 journal 恢复走全新路径 + §5.3 bash 草案降级算法伪代码 + §5.4 Git tag 从 meta.json 读取 + §十一 v24 退出码子命令作用域契约）
- `docs/planning/030-mapping-table.md`（O-1 v12：P5/阶段一/阶段三 三列分离 +
  F-only excluded-deferred 隔离 + reverse-lookup 路径；v12 仅配套标签同步 per codex v12 P2 #5；v24 未修改）
- **O-2 v24 9 文件输出**（待阶段一启动后生成；v7 per codex v6 P1 #5 + v8 per codex v7 P1 #6 + v10 per codex v9 P1 #2 + P1 #3 + P2 #6 + v11 per codex v10 P1 #1 + codex v11 P1 #1 备份-回滚-journal + 故障注入 7 场景 + codex v11 P1 #2 CLI 解析 + codex v11 P2 #4 退出码作用域契约 + codex v12 P1 #1-#4 持久化事务目录 + 结构化 journal + 故障注入 8 场景 + codex v13 P1 #1+#2 + P2 #3 排他锁 + 路径约束 + 旧对完整性 + 故障注入 10 场景 + codex v14 P1 #1+#2 锁时序 + 三者事实一致性 + 故障注入 11 场景 + codex v15 P1 #1+#2 旧对精确指纹 + reconcile 子命令入口 + 故障注入 12 场景 + codex v16 P1 #1+#2 + P2 #3 共享分类 + 状态不变量 + 旧对自洽 + codex v17 P1 #1+#2 + P2 #3 幂等 no-op + selfcheck fail-closed + 故障注入 16 场景 + codex v18 P1 #1+#2 + P2 #3 fsync 状态依赖数据屏障 + Python 实现契约 + no-op 严格 sidecar 校验 + 故障注入 22 场景 + codex v19 P1 #1 + P2 #2 跨目录 rename 双目录同步 + 3 断电窗口场景 + 故障注入 25 场景 + reconcile 标签统一 + codex v20 P1 #1+#2+#3 rolling_back 白名单 + 无 journal 走全新路径 + verified 清理 .txn 屏障 + 3 新场景 + 故障注入 28 场景 + codex v21 P1 #1 白名单可执行等价判定 + codex v22 P1 #1 回滚还原逐次 mv 屏障 + codex v23 P1 #1+P2 #2 回滚段 fsync 退出码统一 exit 5 + 回滚还原 fsync 窗口场景 + 故障注入 29 场景）：
  - 8 稳定证据文件（构成"SHA 一致"判定基础）：
    - `docs/planning/evidence/d-stage-audit.tsv`（主表 6 字段）
    - `docs/planning/evidence/d-stage-audit.tsv.sha256`（主表 hash）
    - `docs/planning/evidence/d-stage-audit.symlink.tsv`（专表 7 字段 symlink，**9 类** v7）
    - `docs/planning/evidence/d-stage-audit.symlink.tsv.sha256`（专表 hash）
    - `docs/planning/evidence/d-stage-audit.D.manifest.tsv`（D 源树 tree-manifest 输入 lock 1）
    - `docs/planning/evidence/d-stage-audit.D.manifest.tsv.sha256`（D manifest hash）
    - `docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv`（D_stage 源树 tree-manifest 输入 lock 2）
    - `docs/planning/evidence/d-stage-audit.D_stage.manifest.tsv.sha256`（D_stage manifest hash）
  - 1 个运行时元数据（不计 SHA 一致判定）：
    - `docs/planning/evidence/d-stage-audit.genesis.json`（**不含** f_only_exclusion_check 字段）
- `docs/planning/evidence/4.0.2-i3/`（阶段二验证证据 + **`d-stage-snapshot.tar.zst` +
  `.sha256`** **v24 per codex v7 P1 #7 + codex v8 P1 #1 + P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v9 P1 #2 + P1 #3 + P1 #4 + P1 #5 + P2 #6 + codex v10 P1 #1+#2 + codex v11 P1 #1+#2 + codex v12 P1 #1-#4 + codex v13 P1 #1+#2 + P2 #3 + codex v14 P1 #1+#2 + codex v15 P1 #1+#2 + codex v16 P1 #1+#2 + P2 #3 + codex v17 P1 #1+#2 + P2 #3 + codex v18 P1 #1+#2 + P2 #3 + codex v19 P1 #1 + P2 #2 + codex v20 P1 #1+#2+#3 + codex v21 P1 #1 + codex v22 P1 #1 + codex v23 P1 #1+P2 #2** D_stage 快照嵌入 release commit + tar.zst
  可复现规范详 框架 §五.3 v24；目录级 staging + reconcile-first + 排他锁（先于一切变更）+ 持久化事务目录 + 结构化 journal（含旧对精确指纹）+ staging_dir 路径约束 + 三者事实一致性（指纹严格匹配 + 共享分类函数）+ 状态专属不变量 + 旧对自洽校验 + 旧对完整性 fail-closed + 幂等 no-op（复用严格 sidecar 校验）+ fsync 状态依赖数据屏障（跨目录 rename 源、目标双目录同步；verified 清理 .txn 屏障；回滚还原逐次 mv 屏障）+ rolling_back 合法中途态白名单（集合成员判断可执行等价判定）+ 无 journal 恢复走全新路径 + `realpath -m` canonicalize + 精确版本锁 tar 1.35 + zstd 1.5.7；待阶段二启动后生成）
- `docs/planning/evidence/o-stage/`（阶段三验证证据，待阶段三启动后生成）
- **`/tmp/r16-d-stage/`**（**v24 唯一合法** D_stage 物化根；系统 `$TMPDIR`，
  不在仓库任何路径下；详 框架 §〇.5 + §四.1 + §五.3 v24）
- `docs/planning/evidence/4.0.2-i3/4.0.2-i3.meta.json`（阶段二 release 元数据，
  **v10 per codex v7 P2 #8 + codex v9 P2 #7** Git tag 名称从 `git_tag_ref` 字段读取
  （dsh 确认），禁止硬编码包版本号 `4.0.2-i3`；**v11 per codex v10 P1 #3 路径统一到
  evidence dir**；待阶段二启动后生成）
- `tools/d-stage-audit-gen.py`（O-2 生成脚本，待 O-1 全闭合 + tree-manifest 锁定后实现）
- `collab/R16-2026-09-03-基座更新迭代评估/{qoder-notes,report}.md`