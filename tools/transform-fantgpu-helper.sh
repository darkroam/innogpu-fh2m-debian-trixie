#!/usr/bin/env bash
# tools/transform-fantgpu-helper.sh — F 血统 helper 打包变换（stdin → stdout）
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §四（share/命令名血统
# 参数化，codex 初审 P1-2 修复）：F 包内 helper 内部不得再引用 innogpu-*
# 命令名或旧 share 路径（否则命令不存在而失败）。只变换**调用链 token**：
#   1. 命令名 innogpu-disable-incompatible-userspace /
#      innogpu-repair-dri-nodes（含 .service 名与 /usr/local/sbin 链接路径）
#   2. helper 解析路径 usr/share/innogpu-fh2m-trixie →
#      usr/share/fantgpu-fh2m-trixie
#   3. install-dri-node-repair-service 的模块条件
#      /sys/module/innogpu → /sys/module/fantgpu（F 内核模块名）
# 其余 innogpu token（O 遗留 loader/conf 清理清单、日志名、诊断 grep）保持
# 不动——它们属「O 遗留物清理/诊断」语义，不参与 F 包调用链。
# O 血统不调用本工具（打包字节与 scripts/ 源码一致，check-release-package
# 的 cmp 契约保持）。

sed -e 's/innogpu-disable-incompatible-userspace/fantgpu-disable-incompatible-userspace/g' \
    -e 's/innogpu-repair-dri-nodes/fantgpu-repair-dri-nodes/g' \
    -e 's#usr/share/innogpu-fh2m-trixie#usr/share/fantgpu-fh2m-trixie#g' \
    -e 's#ConditionPathExists=/sys/module/innogpu#ConditionPathExists=/sys/module/fantgpu#g'
