# R5 诊断脚本闸门缺陷重蹈

## 现象

R5 诊断内核的 step-7/step-8 控制脚本复核中发现 `require_local_tty` 的调用方式会把真实本地会话拒绝掉；r5dpm2 首启健康门旧稿再次出现同形错误。旧稿使用：

```bash
terminal=$(require_local_tty)
```

命令替换把函数标准输出变成管道，函数内部的 `[[ -t 1 ]]` 因而恒假；函数本身通过全局变量传回终端，也没有可供捕获的 stdout。现场错误为 `ERROR: stdin/stdout must be a local TTY`。这是脚本闸门缺陷，不是物理 TTY 不存在。

同一批还暴露了两个采集层问题：sudo 的 `use_pty` 使进程内 `tty` 看到 `/dev/pts/*` 且 `SUDO_TTY` 未设置，和日志采集把带连字符的 boot ID 直接传给 `journalctl -b`，生成 `Failed to add match ...: Invalid argument` 的 77B 错误桩。

## 证据

- R16 原文快照：`.runtime-archive/r17-docs/archive-originals/R16/R16-originals.tar @ fee2c89273b5` 内 `./qoder-notes.md#L8646-L8654`、`./report.md#L10975-L10981` 记录 step-7 闸门、sudo PTY、journal boot ID 三项缺陷及最终处理；`./report.md#L11155-L11187`、`./qoder-notes.md#L8824-L8875` 记录首启旧稿 `f650bfe8…` 到 `4945e8e2…` 的同形命令替换重蹈。
- step-7 tracked 结果：`docs/planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-watchdog-step7-result.txt#L48-L60 @ ce1426eebe7d`，实测文件 SHA-256 `efc05540b8c3`；结果逐字保留 `FAILED_INVALID_BOOT_ID_FORMAT`、三项 `execution_defect_*`、`experiment_rerun=no` 和错误原件保留状态。
- step-8 tracked 结果：`docs/planning/evidence/o-stage/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-step8-result.txt#L68-L71 @ c3105ec2670f`、`#L111-L121`，实测文件 SHA-256 `65705692c49f`；其中偏差、`repeat=forbidden`、`R5=FAIL`、`U1/U2=NOT_RUN_FROZEN`、`validation_results=FROZEN_UNSIGNED` 和 `tag=NOT_CREATED` 均保留。
- 最终脚本归档实测：step-7 五件 SHA-12 为 `d9bf0bfe4c46`、`2b1537b9479a`、`6d3ad1479568`、`e1897b54b628`、`b92512cfb68c`；step-8 五件为 `b57eac466dba`、`1da1b45f0168`、`3359294fa156`、`e7fd112edc13`、`efeb42a7defd`。首启修正版 `.runtime-archive/runtime-5.0.0-i6/r5-dpm-prepare-watchdog-kernel-firstboot/control/10-firstboot-health.sh @ 4945e8e2982a`；journal 修复记录 `.runtime-archive/runtime-5.0.0-i6/r5-dpm-watchdog-step7-collect/journal-collection-repair.txt @ 02b0c59ae836`。

## 根因/排除项

命令替换创建子 shell，并把 stdout 接到捕获管道；因此闸门函数检查的是管道而不是操作者的 TTY。这个语义错误可以通过 `bash -n` 和普通 shellcheck，不能通过语法门发现。sudo PTY 的 `/dev/pts/*` 现象也不能单独推出操作者在 SSH：物理会话事实应由 `loginctl show-session self -p TTY -p Remote -p Type` 读取，并与 `-t 0/-t 1` 和 SSH 环境禁令合并。

boot ID 采集失败是格式缺陷，不是测试失败。首次 journal 产物和 77B 错误桩必须保留；修复只把连字符去掉后在同一 boot 补采，未重跑实验。修复后的 kernel/full journal SHA-12 分别为 `4af7d2753722`、`9afa5bc7e5f3`。

## 修复或边界

首启修正版改为直接调用并使用函数写入的全局变量：

```bash
terminal=
require_local_tty
```

最终门保留 `-t 0/-t 1`、物理会话的 TTY/Remote/Type 三条件和 SSH 环境断言；step-7/step-8 的最终脚本哈希被单独锚定。dsh 接受的脚本漂移只涉及工具/闸门层，未弱化触发写路径，且 `experiment_rerun=no`；这不把旧稿错误改写成“从未存在”。

共同教训是：闸门函数必须直接调用，不能写成 `$(gate_fn)`；每项修复必须同时记录事实、处置、教训和验证；“修复要求”与“已落实”必须分层。后续检查还必须验证最终文件的调用点，而不是只验证函数体或 `bash -n`。

## 后续门槛

每次脚本进入冻结状态前，至少执行调用点 grep、PTY 实测、`loginctl` 会话复核、boot ID 归一化复核，并对错误桩做 SHA 归档。采集器若出现 `FAILED_OR_UNVERIFIED`、错误 boot ID 或缺少真实 trigger marker，必须停止收集和判定，不得写成 `NOT_REPRODUCED`。

本事故不改变实验裁定：`OUTSIDE_COVERAGE`、`r5_root_cause=unresolved`、`R5=FAIL`、`禁止重跑`、`U1/U2`、`validation-results`、`签发`、`tag` 均保持原值。历史 dsh “接受”只接受证据和边界，不追溯为事前放行。

## 验证边界

修复后的脚本和 journal 采集链通过了对应静态与实测门，但这只能证明闸门和取证工具行为，不能证明 DPM watchdog 动态触发，也不能证明挂死位置。错误桩、旧稿 SHA 和执行偏差仍是事故资产；`experiment_rerun=no` 与 `repeat=forbidden` 不得删除。

## 回退条件

若任何闸门再次在真实本地 TTY 拒绝、`loginctl` 与 stdio 事实冲突、boot ID 归一化后仍失败，或 trigger marker 不完整，应停在闸门阶段，保留原始和修复产物，不执行 `pm_test`，也不把失败写成未复现。需要新实验时必须另立设计、审查和放行链；本轮冻结项不解除。
