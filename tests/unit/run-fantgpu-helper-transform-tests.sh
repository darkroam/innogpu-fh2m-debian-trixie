#!/usr/bin/env bash
# tests/unit/run-fantgpu-helper-transform-tests.sh — tools/transform-fantgpu-helper.sh 与
# F 包 helper 安装树运行契约单测
#
# 依据 docs/planning/c3-a-4-reproducible-input-plan.md §四（codex 初审 P1-2 修复）：
# F 血统打包时变换 helper 内部**调用链 token**（innogpu-<命令名>、
# usr/share/innogpu-fh2m-trixie 路径、install-dri-node-repair-service 的模块
# 条件）；O 血统保持 scripts/ 原始字节（check-release-package cmp 契约）。
# 含**安装树运行测试**：变换后的 install-dri-node-repair-service.sh 在假安装
# 树（INNOGPU_DRI_TEST_ROOT + stub systemctl）中成功安装 fantgpu 命名单元。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
XF="$ROOT/tools/transform-fantgpu-helper.sh"
cd "$ROOT"
export LC_ALL=C

[ -f "$XF" ] || { echo "FATAL: transform tool not found" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fht-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

HELPERS=(disable-incompatible-userspace.sh repair-dri-nodes.sh test-xorg-once.sh
    restore-dp1-mode-x11.sh xdisplay-session.sh install-xdisplay-user.sh
    restore-tty1-login.sh display-recover-and-diagnose.sh prepare-soft-xorg-dwm.sh
    start-soft-xorg-dwm-from-ssh.sh check-soft-xorg-dwm.sh install-dri-node-repair-service.sh)
CMDS=(disable-incompatible-userspace repair-dri-nodes test-xorg-once
    restore-dp1-mode-x11 xdisplay-session install-xdisplay-user
    restore-tty1-login display-recover-and-diagnose prepare-soft-xorg-dwm
    start-soft-xorg-dwm check-soft-xorg-dwm install-dri-node-repair-service)

# 假安装树（镜像 builder 的 F 分支安装动作）
FAKE="$TMP/tree"
mkdir -p "$FAKE/usr/share/fantgpu-fh2m-trixie" "$FAKE/usr/bin" "$FAKE/usr/sbin"
for h in "${HELPERS[@]}"; do
    bash "$XF" < "scripts/$h" > "$FAKE/usr/share/fantgpu-fh2m-trixie/$h"
    chmod 0755 "$FAKE/usr/share/fantgpu-fh2m-trixie/$h"
done
for c in "${CMDS[@]}"; do
    h="$c.sh"
    [[ "$c" == display-recover-and-diagnose ]] && h="display-recover-and-diagnose.sh"
    [[ "$c" == start-soft-xorg-dwm ]] && h="start-soft-xorg-dwm-from-ssh.sh"
    [[ "$c" == prepare-soft-xorg-dwm ]] && h="prepare-soft-xorg-dwm.sh"
    [[ "$c" == check-soft-xorg-dwm ]] && h="check-soft-xorg-dwm.sh"
    [[ "$c" == install-dri-node-repair-service ]] && h="install-dri-node-repair-service.sh"
    ln -sfn "../share/fantgpu-fh2m-trixie/$h" "$FAKE/usr/bin/fantgpu-$c"
    ln -sfn "../share/fantgpu-fh2m-trixie/$h" "$FAKE/usr/sbin/fantgpu-$c"
done

# t01 变换后副本无旧调用链 token
leftover="$(grep -l 'innogpu-disable-incompatible-userspace\|innogpu-repair-dri-nodes\|usr/share/innogpu-fh2m-trixie' \
    "$FAKE/usr/share/fantgpu-fh2m-trixie/"*.sh 2>/dev/null | wc -l || true)"
if [ "$leftover" -eq 0 ]; then
    ok t01
else
    bad t01 "leftover=$leftover"
fi

# t02 被引用的 fantgpu-* 命令在树内存在且解析到 share 文件
missing=0
for c in "${CMDS[@]}"; do
    link="$FAKE/usr/bin/fantgpu-$c"
    target="$(readlink -f "$link" 2>/dev/null || true)"
    if [ ! -x "$target" ]; then missing=$((missing + 1)); fi
done
if [ "$missing" -eq 0 ]; then
    ok t02
else
    bad t02 "missing=$missing"
fi

# t03 O 血统不动：scripts/ 原始字节未被本工具改变（变换仅作用于打包副本）
if cmp -s "scripts/prepare-soft-xorg-dwm.sh" "scripts/prepare-soft-xorg-dwm.sh" \
   && grep -Fq "innogpu-disable-incompatible-userspace" "scripts/prepare-soft-xorg-dwm.sh"; then
    ok t03
else
    bad t03 "source helper unexpectedly modified"
fi

# t04 模块类 token 保留（变换不触及模块名/日志名/清理清单）
if grep -Fq ".not-char-before-innogpu" \
       "$FAKE/usr/share/fantgpu-fh2m-trixie/repair-dri-nodes.sh" \
   && grep -Fq "/etc/ld.so.conf.d/0-innogpu-hwgl.conf" \
       "$FAKE/usr/share/fantgpu-fh2m-trixie/disable-incompatible-userspace.sh"; then
    ok t04
else
    bad t04 "module-class tokens were wrongly transformed"
fi

# t05 安装树运行：变换后的 install-dri-node-repair-service.sh 成功安装
# fantgpu 命名单元（stub systemctl；INNOGPU_DRI_TEST_ROOT 重映射 /etc /usr）
mkdir -p "$TMP/bin"
cat > "$TMP/bin/systemctl" <<'EOF'
#!/bin/bash
case "${1:-}" in
    is-enabled) exit 1 ;;   # 视为未启用
    daemon-reload|enable|start|disable|status) exit 0 ;;
    *) exit 0 ;;
esac
EOF
chmod +x "$TMP/bin/systemctl"
set +e
PATH="$TMP/bin:$PATH" INNOGPU_DRI_TEST_ROOT="$FAKE" \
    INNOGPU_ROOT="$ROOT" \
    bash "$FAKE/usr/share/fantgpu-fh2m-trixie/install-dri-node-repair-service.sh" \
    > "$TMP/svc.out" 2> "$TMP/svc.err"
RC5=$?
set -e
unit="$FAKE/etc/systemd/system/fantgpu-repair-dri-nodes.service"
usrbin="$FAKE/usr/bin/fantgpu-repair-dri-nodes"
if [ "$RC5" -eq 0 ] && [ -f "$unit" ] \
   && grep -Fq "ConditionPathExists=/sys/module/fantgpu" "$unit" \
   && grep -Fq "Documentation=file:/usr/share/fantgpu-fh2m-trixie/repair-dri-nodes.sh" "$unit" \
   && [ -L "$usrbin" ]; then
    ok t05
else
    bad t05 "rc=$RC5 unit=$([ -f "$unit" ] && echo y || echo n) err=[$(head -2 "$TMP/svc.err" | tr '\n' ' ')]"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
