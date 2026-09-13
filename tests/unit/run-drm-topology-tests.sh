#!/usr/bin/env bash
# tests/unit/run-drm-topology-tests.sh — F5 probe-drm-topology.c 扩展单测
#
# 契约（validation-plan §〇 F5 自测行 + §1.2 R7-R9 证据格式）：假 connector 树
# + 假 /sys/class/backlight 关联（含 unresolved 行）：输出格式逐行契约断言
# （connector/mode/backlight 关联/DDCCI 属性段）。夹具构建输出新段全部
# fixture_ 命名空间；生产构建仅编译验证（探针只读，但真机拓扑不属单测）。
# 退出码：0=全过 1=用例失败 2=环境错误。

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SRC="$ROOT/tools/probe-drm-topology.c"
cd "$ROOT"
export LC_ALL=C

[ -f "$SRC" ] || { echo "FATAL: probe-drm-topology.c not found" >&2; exit 2; }
command -v gcc >/dev/null 2>&1 || { echo "FATAL: gcc required" >&2; exit 2; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/drmtop-tests.XXXXXX")"
trap 'rm -rf -- "$TMP"' EXIT INT TERM HUP

PASS=0; FAILN=0
ok()  { PASS=$((PASS + 1)); echo "ok  $1"; }
bad() { FAILN=$((FAILN + 1)); echo "BAD $1: $2" >&2; }

gcc -O2 -Wall -Wextra -DINNOGPU_DMABUF_FIXTURE_HOOKS -o "$TMP/probe-fx" "$SRC" \
    || { echo "FATAL: fixture build failed" >&2; exit 2; }
gcc -O2 -Wall -Wextra -o "$TMP/probe-prod" "$SRC" \
    || { echo "FATAL: production build failed" >&2; exit 2; }

# 假 backlight 树：intel_backlight → drm connector 路径（关联成功）；
# acpi_video0 → 非 drm 平台设备（必须 unresolved，禁止全局误归因）；
# fake_bl → 含 /drm/ 但形态不符 card<u>/card<u>-<conn> 的路径（必须
# unresolved，禁止路径判断过宽误归因，codex 复审 P2-5）；
# fake_bl2 → card 编号不一致 card0/card1-eDP-1（codex 复审 P2 ca==cb）；
# fake_bl3 → connector 名后带额外路径组件（codex 复审 P2 路径结尾校验）
mkdir -p "$TMP/bl/class/backlight/intel_backlight" \
         "$TMP/bl/class/backlight/acpi_video0" \
         "$TMP/bl/class/backlight/fake_bl" \
         "$TMP/bl/class/backlight/fake_bl2" \
         "$TMP/bl/class/backlight/fake_bl3" \
         "$TMP/bl/devices/pci0000:00/0000:00:02.0/drm/card0/card0-eDP-1" \
         "$TMP/bl/platform/fake-led" \
         "$TMP/bl/devices/fake/drm/card0/not-connector" \
         "$TMP/bl/devices/fake2/drm/card0/card1-eDP-1" \
         "$TMP/bl/devices/fake3/drm/card0/card0-eDP-1/sub"
ln -sfn ../../../devices/pci0000:00/0000:00:02.0/drm/card0/card0-eDP-1 \
    "$TMP/bl/class/backlight/intel_backlight/device"
ln -sfn ../../../platform/fake-led \
    "$TMP/bl/class/backlight/acpi_video0/device"
ln -sfn ../../../devices/fake/drm/card0/not-connector \
    "$TMP/bl/class/backlight/fake_bl/device"
ln -sfn ../../../devices/fake2/drm/card0/card1-eDP-1 \
    "$TMP/bl/class/backlight/fake_bl2/device"
ln -sfn ../../../devices/fake3/drm/card0/card0-eDP-1/sub \
    "$TMP/bl/class/backlight/fake_bl3/device"

run_fixture() {  # run_fixture [backlight-root]
    local bl="${1:-}"
    set +e
    if [[ -n "$bl" ]]; then
        INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_BACKLIGHT_ROOT="$bl" \
            "$TMP/probe-fx" > "$TMP/fx.out" 2> "$TMP/fx.err"
    else
        INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 \
            "$TMP/probe-fx" > "$TMP/fx.out" 2> "$TMP/fx.err"
    fi
    RC=$?
    set -e
}

# t01 connector/mode/DDCCI 契约段逐行断言（R7-R9 证据格式）
run_fixture
if [ "$RC" -eq 0 ] \
   && grep -Fxq '  fixture_connector 51 eDP-1 status=connected modes=2' "$TMP/fx.out" \
   && grep -Fxq '  fixture_1920x1200@60' "$TMP/fx.out" \
   && grep -Fxq '  fixture_1920x1200@48' "$TMP/fx.out" \
   && grep -Fxq '  fixture_connector 51 ddcci-props=none' "$TMP/fx.out" \
   && grep -Fxq '  fixture_connector 52 HDMI-A-1 status=disconnected modes=0' "$TMP/fx.out" \
   && grep -Fxq '  fixture_connector 52 ddcci-props=none' "$TMP/fx.out" \
   && grep -Fxq 'fixture_topology_collect=complete' "$TMP/fx.out"; then
    ok t01
else
    bad t01 "rc=$RC out=[$(tail -6 "$TMP/fx.out" | tr '\n' ';')]"
fi

# t02 backlight 关联表：drm 路径解析成功 + 非 drm 设备/形态不符路径 unresolved
run_fixture "$TMP/bl/class/backlight"
if [ "$RC" -eq 0 ] \
   && grep -Fq '  fixture_intel_backlight -> ' "$TMP/fx.out" \
   && grep -Fq '/drm/card0/card0-eDP-1' "$TMP/fx.out" \
   && grep -Fxq '  fixture_acpi_video0 -> unresolved' "$TMP/fx.out" \
   && grep -Fxq '  fixture_fake_bl -> unresolved' "$TMP/fx.out" \
   && grep -Fxq '  fixture_fake_bl2 -> unresolved' "$TMP/fx.out" \
   && grep -Fxq '  fixture_fake_bl3 -> unresolved' "$TMP/fx.out"; then
    ok t02
else
    bad t02 "rc=$RC out=[$(grep -E 'backlight|-> ' "$TMP/fx.out" | tr '\n' ';')]"
fi

# t03 命名空间：新段（connector 契约行/mode 行/backlight 行）必须全部 fixture_ 前缀
if ! grep -E '^  connector ' "$TMP/fx.out" | grep -qv 'fixture_connector' \
   && ! grep -E '^  [0-9]+x[0-9]+@[0-9]+$' "$TMP/fx.out" | grep -qv 'fixture_' \
   && ! grep -E '^  [A-Za-z0-9_.-]+ -> ' "$TMP/fx.out" | grep -qv 'fixture_'; then
    ok t03
else
    bad t03 "unprefixed F5 contract line leaked from fixture build"
fi

# t04 静态：DDCCI 属性匹配（(?i)ddcci）/symlink 解析/禁止全局误归因/unresolved 语义
if grep -Fq 'strcasestr(name, "ddcci")' "$SRC" \
   && grep -Fq 'DRM_IOCTL_MODE_OBJ_GETPROPERTIES' "$SRC" \
   && grep -Fq 'realpath(' "$SRC" \
   && grep -Fq -- '-> unresolved' "$SRC" \
   && grep -Fq 'strstr(rp, "/drm/")' "$SRC" \
   && grep -Fq 'connector_type_name' "$SRC"; then
    ok t04
else
    bad t04 "F5 static contract elements missing"
fi

# t05 静态：单一源码双构建（夹具宏守卫 + 环境变量开关；生产构建宏无效）
if grep -Fq '#ifdef INNOGPU_DMABUF_FIXTURE_HOOKS' "$SRC" \
   && grep -Fq 'getenv("INNOGPU_DMABUF_TOPOLOGY_FIXTURE")' "$SRC" \
   && grep -Fq 'INNOGPU_DMABUF_BACKLIGHT_ROOT' "$SRC"; then
    ok t05
else
    bad t05 "dual-build fixture convention missing"
fi

# t06 采集失败不得伪装（codex 初审 P1-3 负向）：modes/ddcci 采集失败 →
# unavailable 输出 + rc=1
set +e
INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_TOPOLOGY_FIXTURE_FAIL=1 \
    "$TMP/probe-fx" > "$TMP/fail.out" 2> "$TMP/fail.err"
RC6=$?
set -e
if [ "$RC6" -eq 1 ] \
   && grep -Fq 'fixture_connector 51 eDP-1 status=connected modes=unavailable' "$TMP/fail.out" \
   && grep -Fq 'fixture_connector 51 ddcci-props=unavailable' "$TMP/fail.out" \
   && grep -Fq 'fixture_topology_collect=partial' "$TMP/fail.out"; then
    ok t06
else
    bad t06 "rc=$RC6 out=[$(tail -4 "$TMP/fail.out" | tr '\n' ';')]"
fi

# t07 静态：采集失败语义（unavailable 输出 + collect_failed → rc≠0；无伪装的 none/0）
if grep -Fq 'modes=unavailable' "$SRC" \
   && grep -Fq 'ddcci-props=unavailable' "$SRC" \
   && grep -Fq 'collect_failed' "$SRC" \
   && grep -Fq 'result = collect_failed ? 1 : 0' "$SRC" \
   && grep -Fq 'topology_collect=%s' "$SRC" \
   && grep -Fq 'prop_fetch_failed' "$SRC"; then
    ok t07
else
    bad t07 "collect-failure semantics missing"
fi

# t08 聚合运行模式（CRTC_ONLY）：rc=0 + 仅 CRTC 段 + complete（R7-R9 证据
# 运行不得设置该模式；该模式供 dmabuf R3+R6 拓扑门禁使用）
set +e
INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY=1 \
    "$TMP/probe-fx" > "$TMP/crtc.out" 2> "$TMP/crtc.err"
RC8=$?
set -e
if [ "$RC8" -eq 0 ] \
   && grep -Fxq 'fixture_topology_collect=complete' "$TMP/crtc.out" \
   && grep -Fq '  index=1 id=11 active=yes' "$TMP/crtc.out" \
   && ! grep -q 'fixture_connector' "$TMP/crtc.out" \
   && ! grep -q 'Backlight:' "$TMP/crtc.out"; then
    ok t08
else
    bad t08 "rc=$RC8 out=[$(cat "$TMP/crtc.out" | tr '\n' ';')]"
fi

# t09 backlight 目录不可读（codex 复审 P2-5 负向）：显式 unavailable + partial + rc=1
set +e
INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_BACKLIGHT_ROOT=/nonexistent/bl \
    "$TMP/probe-fx" > "$TMP/blfail.out" 2> "$TMP/blfail.err"
RC9=$?
set -e
if [ "$RC9" -eq 1 ] \
   && grep -Fq 'fixture_backlight=unavailable (opendir' "$TMP/blfail.out" \
   && grep -Fq 'fixture_topology_collect=partial' "$TMP/blfail.out"; then
    ok t09
else
    bad t09 "rc=$RC9 out=[$(tail -3 "$TMP/blfail.out" | tr '\n' ';')]"
fi

# t10 静态：backlight 路径形态限定（/drm/card<u>/card<u>-<connector>；opendir
# 失败 → collect_failed）
if grep -Fq 'sscanf(p, "/drm/card%u/card%u-%63[^/]%n"' "$SRC" \
   && grep -Fq 'ca == cb' "$SRC" \
   && grep -Fq 'p[consumed]' "$SRC" \
   && grep -Fq 'backlight=unavailable (opendir' "$SRC"; then
    ok t10
else
    bad t10 "backlight path-shape restriction missing"
fi

# t11 CRTC_ONLY=0 负向（codex 复审 P2）：=0 不得绕过 F5 采集，输出完整
# 契约段 + Backlight: + complete + rc=0
set +e
INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY=0 \
    "$TMP/probe-fx" > "$TMP/c0.out" 2> "$TMP/c0.err"
RC11=$?
set -e
if [ "$RC11" -eq 0 ] \
   && grep -Fq "  fixture_connector 51 eDP-1 status=connected" "$TMP/c0.out" \
   && grep -Fq "Backlight:" "$TMP/c0.out" \
   && grep -Fxq "fixture_topology_collect=complete" "$TMP/c0.out"; then
    ok t11
else
    bad t11 "rc=$RC11 out=[$(tail -3 "$TMP/c0.out" | tr '\n' ';')]"
fi

# t12 CRTC_ONLY 空值负向（codex 复审 P2）：空值等同未设置，不得绕过 F5 采集
set +e
INNOGPU_DMABUF_TOPOLOGY_FIXTURE=1 INNOGPU_DMABUF_TOPOLOGY_CRTC_ONLY= \
    "$TMP/probe-fx" > "$TMP/ce.out" 2> "$TMP/ce.err"
RC12=$?
set -e
if [ "$RC12" -eq 0 ] \
   && grep -Fq "  fixture_connector 51 eDP-1 status=connected" "$TMP/ce.out" \
   && grep -Fxq "fixture_topology_collect=complete" "$TMP/ce.out"; then
    ok t12
else
    bad t12 "rc=$RC12 out=[$(tail -3 "$TMP/ce.out" | tr '\n' ';')]"
fi

echo "PASS=$PASS FAIL=$FAILN"
[ "$FAILN" -eq 0 ] || exit 1
exit 0
