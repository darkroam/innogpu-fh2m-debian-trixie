#!/usr/bin/env bash
# tests/unit/run-d-stage-audit-fault-tests.sh — O-2 snapshot 29 场景故障注入测试入口
#
# 薄包装：LC_ALL=C + python3 harness；退出码透传
#   harness：tests/unit/d-stage-audit-fault-tests.py
#   （17 单元场景 + 8 补充回归本机直跑；12 power-loss 场景强制断电语义，
#   由 VM 驱动经 --vm-prepare / --vm-mark / --vm-setup-injection / --vm-inject /
#   --vm-verify / --vm-summarize 六模式执行，注入点为工具内接缝 + 阻塞握手，
#   验证由 harness 重启后独立完成；未执行时如实 UNVERIFIED）
# 未经全部场景通过的 snapshot 子命令禁止在阶段二 release commit 中使用。
set -u
LC_ALL=C
export LC_ALL
cd "$(dirname "$0")/../.." || exit 2
exec python3 tests/unit/d-stage-audit-fault-tests.py "$@"
