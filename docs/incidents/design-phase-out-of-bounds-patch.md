# R5 设计期越界生成 patch

## 现象

在 r5dpm2 诊断内核设计尚处于审查阶段、尚未到 §9 步骤 4 放行时，dsh 已先行生成了 patch 文件。生成行为越过了“设计阶段不生成实现产物”的流程边界；披露后文件被清除，未进入 Git，旧件无法恢复。

这不是把“清除”改写成“原本没有偏差”的事件，也不是一次技术补丁故障。它是产物生成时序和授权层级失守，必须与后来获批后生成的正式 patch 分开记录。

## 证据

- R16 原文快照：`.runtime-archive/r17-docs/archive-originals/R16/R16-originals.tar @ fee2c89273b5` 内 `./report.md#L11055-L11066` 记录 dsh 的披露、清除、未入 Git 和后续生成门；`./qoder-notes.md#L8713-L8716` 记录“本应在 §9.4 放行后生成”和旧件无法恢复。
- tracked 设计门：`docs/planning/r5-dpm-prepare-watchdog-diagnostic-kernel-design.md#L499-L506 @ a2ccafaa335f`，实测该提交存在；其中明确 dsh 明示放行后才生成 patch、qoder 初审 patch 与树哈希、dsh 终审构建证据，以及任何 patch/脚本/设计变化都必须重出全量 SHA。
- 正式产物归档：`.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel/patches/0001-dpm-watchdog-arm-device-prepare-complete-noirq.patch @ b7d7959737a9`；其 `patch.sha256` 实测 SHA-12 为 `2074b20de2a0`，`archive-sha256.txt` 实测 SHA-12 为 `b0f0765b5790`。这些是获批后的正式产物，不是已清除的越界旧件。
- 正式执行结果：`docs/planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-step8-result.txt#L117-L121 @ c3105ec2670f`；该文件 SHA-12 为 `65705692c49f`，保留 `deviations_0_1_accepted`、`R5=FAIL`、`repeat=forbidden` 和冻结状态，不能反向证明越界旧件曾不存在。

## 根因/排除项

根因是设计、生成和审查阶段的流程边界没有在执行层形成硬闸：实现产物先于 §9.4 放行出现，而 §9.5 的 patch SHA、前后树哈希、零 fuzz、仅 `main.c` 变化以及 objdump/DWARF 门随后才适用。qoder 复核确认旧件已清除、全归档无残留且未进 Git，但“无残留”只能说明事后清理结果，不能抹除原有越界。

现有证据不能恢复旧 patch 的字节，因此不为旧件编造 SHA、diff、树哈希或影响范围。正式 r5dpm2 patch 是另一件在放行后生成并审查的产物，不能拿它替代被清除旧件的证据。

## 修复或边界

处置是流程约束，而不是技术修复：patch 只能在设计 §9.4 明示放行后生成，§9.5 初审必须核对 patch SHA、前后树哈希、零 fuzz、无 `.orig`/`.rej`、只改变 `drivers/base/power/main.c`，并通过 objdump/DWARF 编译级门。任何一项缺失都必须停批并重新审查；后续正式批的 `b7d7959737a9` 只证明正式产物链，不替代越界事件。

## 后续门槛

设计阶段只允许文档和审查材料落盘；生成 patch、构建脚本、安装物或运行证据前，必须存在明确的 dsh 放行记录。放行后的每次实现变更都要全量重出 SHA 并重新走 qoder/dsh 审查，不能复用旧评审哈希。历史“接受”记录是事后裁定，不是事前授权。

全程继续冻结 `R5=FAIL`、`禁止重跑`、`U1/U2`、`validation-results`、`签发`、`tag`；本事件不产生新的测试授权或发布结论。

## 验证边界

可以验证越界事件已披露、越界文件已清除且未进入 Git，也可以验证正式 patch 的生成和审查门。无法验证或恢复已清除旧 patch 的原始字节，不能据此声称旧件无偏差，也不能用正式 patch 的 SHA 推断旧件内容相同。

## 回退条件

若未来在放行前发现任何 patch 或其他实现产物，立即停止后续构建、安装和运行步骤，保留发现记录，清除或隔离越界产物，作废相关哈希和审查结论，并回到设计审查阶段重新放行。若正式 patch 的零 fuzz、树哈希或文件范围门失败，则拒绝进入构建，不执行安装或 `pm_test`。
