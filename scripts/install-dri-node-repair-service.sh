#!/bin/bash
# Install a small boot-time repair service for systems where innogpu registers
# DRM/fbdev minors in sysfs but devtmpfs/udev misses the /dev nodes.
#
# Trusted helper resolution (an arbitrary PATH hit is never used):
#   1. the packaged helper: /usr/sbin/innogpu-repair-dri-nodes must exist and
#      resolve (readlink -f) to /usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh;
#   2. the source-tree fallback: a symlink in /usr/local/sbin pointing at
#      $ROOT/scripts/repair-dri-nodes.sh (distinct location).
#
# Ownership is classified BEFORE any write: an existing path is absent / owned
# (this project's helper symlink, or a unit carrying the project managed-by
# marker, or a compatible legacy unit structure) / foreign (anything else). A
# foreign helper/unit is refused with ZERO writes.
#
# Everything this run creates, rewrites or enables is one transaction with a
# unified rollback on ANY failure exit (EXIT/ERR traps): created files are
# removed, an owned unit is restored byte-exact from a verified backup, and
# the systemd enable state is restored to the pre-run is-enabled state
# (a unit that was disabled stays disabled; a unit this run enabled is
# disabled again - including after a partially-failed enable that left a
# wants link). The /usr/bin convenience link is optional: its failure only
# warns and never aborts the install.
#
# Unit-test hook (NEVER set in real runs): INNOGPU_DRI_TEST_ROOT must be a
# non-empty absolute path != "/"; it skips the EUID check and remaps /etc and
# /usr under that root. The hook fails closed: with an invalid root the script
# aborts before touching anything.

set -euo pipefail

TEST_ROOT="${INNOGPU_DRI_TEST_ROOT:-}"
PREFIX="${TEST_ROOT:-}"

if [[ -n "$TEST_ROOT" ]]; then
    NORM="$(readlink -f "$TEST_ROOT" 2>/dev/null || true)"
    if [[ -z "$NORM" || "$NORM" == "/" || "$TEST_ROOT" != /* ]]; then
        echo "ERROR: INNOGPU_DRI_TEST_ROOT must be a non-empty absolute path != /" >&2
        exit 1
    fi
    PREFIX="$NORM"
fi

if [[ ${EUID:-$(id -u)} -ne 0 && -z "$TEST_ROOT" ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

SERVICE="$PREFIX/etc/systemd/system/innogpu-repair-dri-nodes.service"
UNIT_NAME="innogpu-repair-dri-nodes.service"
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
UNIT_MANAGED_MARKER='# Managed by innogpu-fh2m-debian-trixie install-dri-node-repair-service.sh'
UNIT_LEGACY_DESC='Description=Repair Innogpu DRM/fbdev device nodes'

# ---- ownership classification (read-only, before any write) ----

fallback_link="$PREFIX/usr/local/sbin/innogpu-repair-dri-nodes"
expected_helper="$(readlink -f "$ROOT/scripts/repair-dri-nodes.sh" 2>/dev/null || true)"

path_state() {
    local path="$1" expected="${2:-}" target
    if [[ -e "$path" || -L "$path" ]]; then
        if [[ -L "$path" ]]; then
            target="$(readlink -f "$path" 2>/dev/null || true)"
            if [[ -n "$target" && -n "$expected" && "$target" == "$expected" ]]; then
                echo owned
            else
                echo foreign
            fi
        else
            echo foreign
        fi
    else
        echo absent
    fi
}

# A project unit must be a regular, non-symlink file carrying the managed-by
# marker; legacy units are accepted only when the whole expected structure
# matches (project description + ExecStart pointing at this project's helper).
is_project_unit() {
    local f="$1" exec_line resolved cand
    [[ -f "$f" && ! -L "$f" ]] || return 1
    grep -Fq "$UNIT_MANAGED_MARKER" "$f" 2>/dev/null && return 0
    grep -Fq "$UNIT_LEGACY_DESC" "$f" 2>/dev/null || return 1
    exec_line="$(sed -n 's/^ExecStart=//p' "$f" 2>/dev/null | head -1)"
    [[ -n "$exec_line" ]] || return 1
    resolved="$(readlink -f "$exec_line" 2>/dev/null || true)"
    for cand in "$expected_helper" "$PREFIX/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"; do
        if [[ -n "$resolved" && "$resolved" == "$(readlink -f "$cand" 2>/dev/null || true)" ]]; then
            return 0
        fi
    done
    return 1
}

expected_pkg="$PREFIX/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh"
pkg_link="$PREFIX/usr/sbin/innogpu-repair-dri-nodes"
helper=""
pkg_mode=0
if [[ -x "$pkg_link" ]]; then
    resolved="$(readlink -f "$pkg_link" 2>/dev/null || true)"
    expected="$(readlink -f "$expected_pkg" 2>/dev/null || true)"
    if [[ -n "$resolved" && "$resolved" == "$expected" ]]; then
        helper="$pkg_link"
        pkg_mode=1
    else
        echo "ERROR: $pkg_link does not resolve to $expected_pkg; refusing to use it" >&2
        exit 1
    fi
fi
if [[ -z "$helper" ]]; then
    if [[ ! -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
        echo "ERROR: innogpu-repair-dri-nodes helper is missing" >&2
        exit 1
    fi
    case "$(path_state "$fallback_link" "$expected_helper")" in
        owned|absent) helper="$fallback_link" ;;
        foreign)
            echo "ERROR: $fallback_link exists and is not this project's helper; refusing to overwrite it" >&2
            exit 1
            ;;
    esac
fi

# unit ownership (foreign -> refuse with zero writes)
unit_action=""
case "$(path_state "$SERVICE")" in
    absent) unit_action=create ;;
    owned) unit_action=backup ;;
    foreign)
        if is_project_unit "$SERVICE"; then unit_action=backup; else
            echo "ERROR: $SERVICE exists and is not this project's unit; refusing to overwrite it" >&2
            exit 1
        fi
        ;;
esac

# /usr/bin convenience link (packaged helper only; optional)
usrbin_link="$PREFIX/usr/bin/innogpu-repair-dri-nodes"
usrbin_action=""
if [[ "$pkg_mode" -eq 1 ]]; then
    case "$(path_state "$usrbin_link" "$expected")" in
        absent) usrbin_action=create ;;
        owned) usrbin_action=keep ;;
        foreign) usrbin_action=warn ;;
    esac
fi

# ---- transaction (all writes below are rolled back on ANY failure) ----
created_helper=""
created_usrbin_link=""
created_unit=""
unit_backup=""
backup_ok=0
SUCCESS=0
ROLLED=0
ENABLE_ATTEMPTED=0
was_enabled=0
if systemctl is-enabled "$UNIT_NAME" >/dev/null 2>&1; then was_enabled=1; fi

RESTORE_FAILED=0
rollback_core() {
    if [[ "$ROLLED" -eq 1 ]]; then return; fi
    ROLLED=1
    # 1) undo the enable state while the unit file still exists, so real
    #    systemd can deterministically clean a partially-created wants/Also
    #    link (disable after removing the unit cannot do that)
    if [[ "$ENABLE_ATTEMPTED" -eq 1 ]]; then
        if [[ "$was_enabled" -eq 1 ]]; then
            systemctl enable "$UNIT_NAME" 2>/dev/null || true
        else
            systemctl disable "$UNIT_NAME" 2>/dev/null || true
        fi
    fi
    # 2) then remove a created unit, or restore a pre-existing unit from its
    #    verified backup; a failed restore keeps the backup and is reported
    if [[ -n "$created_unit" ]]; then
        rm -f "$created_unit" 2>/dev/null || true
    elif [[ "$backup_ok" -eq 1 && -n "$unit_backup" ]]; then
        if ! cp -p "$unit_backup" "$SERVICE" 2>/dev/null; then
            RESTORE_FAILED=1
        fi
    fi
    if [[ -n "$created_helper" && -L "$created_helper" ]]; then rm -f "$created_helper" 2>/dev/null || true; fi
    if [[ -n "$created_usrbin_link" && -L "$created_usrbin_link" ]]; then rm -f "$created_usrbin_link" 2>/dev/null || true; fi
    systemctl daemon-reload 2>/dev/null || true
}

on_exit() {
    if [[ "$SUCCESS" -ne 1 ]]; then
        rollback_core
        if [[ "$RESTORE_FAILED" -eq 1 ]]; then
            echo "ERROR: install failed; could NOT restore $SERVICE." >&2
            echo "The original unit backup is preserved at: $unit_backup" >&2
            echo "Manual recovery: cp -p \"$unit_backup\" \"$SERVICE\" && systemctl daemon-reload" >&2
        else
            echo "ERROR: install failed; changes made by this run were rolled back." >&2
        fi
        echo "Manual recovery if anything remains:" >&2
        echo "  systemctl disable innogpu-repair-dri-nodes.service" >&2
        echo "  rm -f /etc/systemd/system/innogpu-repair-dri-nodes.service" >&2
        echo "  rm -f /usr/local/sbin/innogpu-repair-dri-nodes" >&2
    fi
    if [[ "$RESTORE_FAILED" -ne 1 ]]; then
        [[ -n "$unit_backup" ]] && rm -f "$unit_backup" 2>/dev/null || true
    fi
}
trap 'exit 1' ERR
trap 'on_exit' EXIT

# write helper (only when this run creates it)
if [[ "$pkg_mode" -eq 0 && ! -L "$fallback_link" ]]; then
    install -d -m 0755 "$PREFIX/usr/local/sbin"
    ln -s "$ROOT/scripts/repair-dri-nodes.sh" "$fallback_link"
    created_helper="$fallback_link"
fi

# the helper must now be executable (fresh fallback was just created above)
if [[ -z "$helper" || ! -x "$helper" ]]; then
    echo "ERROR: innogpu-repair-dri-nodes helper is not executable: $helper" >&2
    exit 1
fi

# /usr/bin convenience link: optional; failure only warns
if [[ "$usrbin_action" == create ]]; then
    if ! mkdir -p "$PREFIX/usr/bin" 2>/dev/null; then
        echo "WARNING: cannot create $PREFIX/usr/bin; /usr/bin convenience link skipped" >&2
    elif ln -sf ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh "$usrbin_link"; then
        created_usrbin_link="$usrbin_link"
    else
        echo "WARNING: cannot create /usr/bin convenience link $usrbin_link; continuing without it" >&2
    fi
elif [[ "$usrbin_action" == warn ]]; then
    echo "WARNING: $usrbin_link exists and is not ours; leaving it alone" >&2
fi

# backup an owned unit (a VERIFIED full copy) before rewriting it; a failed
# backup aborts before the unit is touched so the original stays byte-exact
if [[ "$unit_action" == create ]]; then
    mkdir -p "$PREFIX/etc/systemd/system"
    created_unit="$SERVICE"
elif [[ "$unit_action" == backup ]]; then
    unit_backup="$(mktemp "${TMPDIR:-/tmp}/innogpu-unit.XXXXXX" 2>/dev/null || true)"
    if [[ -z "$unit_backup" ]] || ! cp -p "$SERVICE" "$unit_backup"; then
        echo "ERROR: cannot back up existing unit $SERVICE; aborting without touching it" >&2
        exit 1
    fi
    backup_ok=1
fi

cat > "$SERVICE" <<EOF
$UNIT_MANAGED_MARKER
[Unit]
Description=Repair Innogpu DRM/fbdev device nodes
Documentation=file:/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh
DefaultDependencies=no
After=systemd-modules-load.service systemd-udev-settle.service
Before=getty@tty1.service display-manager.service
ConditionPathExists=/sys/module/innogpu

[Service]
Type=oneshot
ExecStart=$helper
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
ENABLE_ATTEMPTED=1
if ! systemctl enable "$UNIT_NAME"; then
    exit 1
fi
if ! systemctl start "$UNIT_NAME"; then
    exit 1
fi

SUCCESS=1

echo
echo "Installed boot-time DRM/fbdev node repair service:"
systemctl --no-pager --full status innogpu-repair-dri-nodes.service || true
echo
echo "Helper: $helper"
echo "Current nodes:"
ls -l "$PREFIX/dev/dri" "$PREFIX/dev/dri/by-path" "$PREFIX/dev/fb0" 2>/dev/null || true
