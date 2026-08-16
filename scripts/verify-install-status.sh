#!/bin/bash
# Read-only installation status verification for any installed package/HWGL state.

set -u -o pipefail

ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
EXPECTED=""
REQUIRE_REBOOT=0
kernel="$(uname -r)"
failures=0

usage() {
    cat <<'USAGE'
Usage: scripts/verify-install-status.sh [--require-reboot] [expected-version]

Without --require-reboot, report installed package, DKMS, driver and desktop
state. With --require-reboot, fail unless the package metadata predates the
current boot and the required runtime driver state is present.
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --require-reboot) REQUIRE_REBOOT=1 ;;
        -h|--help) usage; exit 0 ;;
        --)
            shift
            if [[ $# -eq 1 && -z "$EXPECTED" ]]; then
                EXPECTED="$1"
                shift
            fi
            if [[ $# -gt 0 ]]; then
                echo "ERROR: unexpected argument after --: $1" >&2
                usage >&2
                exit 2
            fi
            break
            ;;
        -*) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
        *)
            if [[ -n "$EXPECTED" ]]; then
                echo "ERROR: expected version was already provided: $EXPECTED" >&2
                usage >&2
                exit 2
            fi
            EXPECTED="$1"
            ;;
    esac
    shift
done

if [[ $# -gt 0 ]]; then
    echo "ERROR: unexpected argument: $1" >&2
    usage >&2
    exit 2
fi

section() {
    printf '\n===== %s =====\n' "$*"
}

pass() {
    printf 'PASS: %s\n' "$*"
}

warn() {
    printf 'WARN: %s\n' "$*"
}

fail() {
    printf 'FAIL: %s\n' "$*"
    failures=$((failures + 1))
}

version="$(dpkg-query -W -f='${Version}' innogpu-fh2m-trixie 2>/dev/null || true)"
process_namespace_unreliable=0
init_cmd="$(tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true)"
if grep -Eq 'bwrap|codex-linux-sandbox' <<<"$init_cmd"; then
    process_namespace_unreliable=1
fi

section "Package"
if [[ -n "$version" ]]; then
    echo "installed_version=$version"
    if [[ -n "$EXPECTED" && "$version" != "$EXPECTED" ]]; then
        fail "expected version $EXPECTED, got $version"
    else
        pass "innogpu-fh2m-trixie is installed"
    fi
else
    fail "innogpu-fh2m-trixie is not installed"
fi

section "Reboot Boundary"
if [[ "$REQUIRE_REBOOT" == "1" ]]; then
    if [[ "$process_namespace_unreliable" == "1" ]]; then
        fail "cannot certify a reboot from an isolated process namespace"
    else
        boot_time="$(uptime -s 2>/dev/null || true)"
        boot_epoch="$(date -d "$boot_time" +%s 2>/dev/null || true)"
        package_record="/var/lib/dpkg/info/innogpu-fh2m-trixie.list"
        package_epoch="$(stat -c '%Y' "$package_record" 2>/dev/null || true)"

        if [[ ! "$boot_epoch" =~ ^[0-9]+$ ]] || [[ ! "$package_epoch" =~ ^[0-9]+$ ]]; then
            fail "cannot determine whether package installation predates this boot"
        elif (( package_epoch >= boot_epoch )); then
            fail "package metadata is newer than this boot; reboot before accepting runtime evidence"
        else
            pass "package metadata predates the current boot"
        fi
    fi
else
    echo "reboot_boundary=NOT_REQUESTED"
fi

section "DKMS"
if command -v dkms >/dev/null 2>&1 || [[ -x /sbin/dkms ]]; then
    dkms_bin="$(command -v dkms || printf '%s\n' /sbin/dkms)"
    "$dkms_bin" status innogpu-kernel || true
    if "$dkms_bin" status innogpu-kernel 2>/dev/null | grep -F "$kernel" | grep -F installed >/dev/null; then
        pass "DKMS module is installed for $kernel"
    else
        fail "DKMS module is not installed for $kernel"
    fi
else
    fail "dkms command is missing"
fi

section "Kernel Module"
lsmod_bin="$(command -v lsmod 2>/dev/null || true)"
if [[ -z "$lsmod_bin" && -x /sbin/lsmod ]]; then
    lsmod_bin=/sbin/lsmod
fi
if [[ -z "$lsmod_bin" ]]; then
    fail "lsmod command is missing"
elif "$lsmod_bin" | awk '$1 == "innogpu" { found = 1 } END { exit found ? 0 : 1 }'; then
    pass "innogpu module is loaded"
else
    if [[ "$REQUIRE_REBOOT" == "1" ]]; then
        fail "innogpu module is not loaded after the required reboot"
    else
        warn "innogpu module is not loaded; reboot or modprobe may be required"
    fi
fi
if [[ -r /proc/driver/innogpu/gpu00/status ]]; then
    cat /proc/driver/innogpu/gpu00/status
    grep -q 'Driver Status:[[:space:]]*OK' /proc/driver/innogpu/gpu00/status && pass "Driver Status OK" || fail "Driver Status is not OK"
    grep -q 'Firmware Status:[[:space:]]*OK' /proc/driver/innogpu/gpu00/status && pass "Firmware Status OK" || fail "Firmware Status is not OK"
else
    if [[ "$REQUIRE_REBOOT" == "1" ]]; then
        fail "/proc/driver/innogpu/gpu00/status is not available after the required reboot"
    else
        warn "/proc/driver/innogpu/gpu00/status is not available"
    fi
fi

if [[ "$REQUIRE_REBOOT" == "1" ]]; then
    modinfo_bin="$(command -v modinfo 2>/dev/null || true)"
    if [[ -z "$modinfo_bin" && -x /sbin/modinfo ]]; then
        modinfo_bin=/sbin/modinfo
    fi
    module_file=""
    if [[ -n "$modinfo_bin" ]]; then
        module_file="$("$modinfo_bin" -F filename innogpu 2>/dev/null || true)"
    fi
    if [[ -z "$modinfo_bin" ]]; then
        fail "modinfo command is missing"
    elif [[ "$module_file" == "/lib/modules/$kernel/"* ]]; then
        pass "innogpu module resolves under the current kernel"
    else
        fail "innogpu module does not resolve under /lib/modules/$kernel"
    fi
fi

section "Device Nodes"
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
if [[ "$process_namespace_unreliable" == "1" ]]; then
    echo "process_namespace=CODEX_OR_BWRAP_ISOLATED_UNRELIABLE"
    [[ -e /dev/dri/card0 ]] && pass "/dev/dri/card0 exists" || warn "/dev/dri/card0 missing in isolated namespace"
    [[ -e /dev/dri/renderD128 ]] && pass "/dev/dri/renderD128 exists" || warn "/dev/dri/renderD128 missing in isolated namespace"
    [[ -e /dev/fb0 ]] && pass "/dev/fb0 exists" || warn "/dev/fb0 missing in isolated namespace"
else
    [[ -e /dev/dri/card0 ]] && pass "/dev/dri/card0 exists" || fail "/dev/dri/card0 missing"
    [[ -e /dev/dri/renderD128 ]] && pass "/dev/dri/renderD128 exists" || fail "/dev/dri/renderD128 missing"
    if [[ -e /dev/fb0 ]]; then
        pass "/dev/fb0 exists"
    elif [[ "$REQUIRE_REBOOT" == "1" ]]; then
        fail "/dev/fb0 missing after the required reboot"
    else
        warn "/dev/fb0 missing"
    fi
fi

section "Boot Autoload"
if grep -qx 'innogpu' /etc/modules-load.d/innogpu.conf 2>/dev/null; then
    pass "boot autoload is enabled"
else
    warn "boot autoload is not enabled in /etc/modules-load.d/innogpu.conf"
fi

section "Xorg And Userspace"
if grep -Eq 'Driver[[:space:]]+"innogpu"' /etc/X11/xorg.conf 2>/dev/null; then
    pass "Xorg persistent config uses innogpu DDX"
elif grep -Eq 'Driver[[:space:]]+"modesetting"' /etc/X11/xorg.conf 2>/dev/null; then
    warn "Xorg persistent config uses modesetting software path"
else
    warn "Xorg persistent driver config is unknown"
fi

hwgl_files=0
for p in \
    /etc/ld.so.conf.d/0-innogpu-hwgl.conf \
    /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
    /usr/lib/x86_64-linux-gnu/gbm/innogpu_gbm.so \
    /usr/lib/xorg/modules/drivers/innogpu_drv.so \
    /usr/share/glvnd/egl_vendor.d/00_inno.json; do
    if [[ -e "$p" || -L "$p" ]]; then
        hwgl_files=$((hwgl_files + 1))
        echo "present: $p"
    else
        echo "missing: $p"
    fi
done
if [[ "$hwgl_files" -eq 5 ]]; then
    pass "Deepin hardware-GL userspace files are installed"
else
    warn "Deepin hardware-GL userspace is incomplete or intentionally disabled"
fi

section "Desktop Runtime"
DISPLAY_NUM="${INNOGPU_X_DISPLAY:-${DISPLAY:-:0}}"
X_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
USER_HOME="${INNOGPU_X_HOME:-$(getent passwd "$X_USER" 2>/dev/null | cut -d: -f6)}"
USER_HOME="${USER_HOME:-$HOME}"
auth=""
for candidate in "${XAUTHORITY:-}" "$HOME/.Xauthority" "$USER_HOME/.Xauthority" /tmp/serverauth.*; do
    [[ -e "$candidate" ]] || continue
    if command -v xdpyinfo >/dev/null 2>&1 &&
       DISPLAY="$DISPLAY_NUM" XAUTHORITY="$candidate" xdpyinfo >/dev/null 2>&1; then
        auth="$candidate"
        break
    fi
done

if [[ -n "$auth" ]]; then
    pass "X display $DISPLAY_NUM is reachable"
    echo "XAUTHORITY=$auth"
    if command -v glxinfo >/dev/null 2>&1; then
        DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" glxinfo -B 2>/dev/null | grep -E 'OpenGL vendor|OpenGL renderer|OpenGL version|Accelerated|Device' || true
        if DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" glxinfo -B 2>/dev/null | grep -Eq 'OpenGL renderer string: Fantasy II-M|OpenGL vendor string: Innosilicon'; then
            pass "desktop OpenGL uses Innosilicon/Fantasy renderer"
        else
            warn "desktop OpenGL is not confirmed as Innosilicon/Fantasy"
        fi
    else
        warn "glxinfo is missing"
    fi
    if command -v xdpyinfo >/dev/null 2>&1; then
        extensions="$(DISPLAY="$DISPLAY_NUM" XAUTHORITY="$auth" xdpyinfo -queryExtensions 2>/dev/null || true)"
        printf '%s\n' "$extensions" | grep -E 'DRI2|DRI3|GLX|Present|RANDR' || true
        grep -q 'DRI3' <<<"$extensions" && pass "DRI3 extension is available" || warn "DRI3 extension is not available"
    fi
else
    warn "X display $DISPLAY_NUM is not reachable from this session"
fi

section "Summary"
if [[ "$failures" -eq 0 ]]; then
    echo "RESULT: PASS_INSTALL_STATUS"
    exit 0
fi

echo "RESULT: FAIL_INSTALL_STATUS failures=$failures"
exit 1
