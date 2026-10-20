#!/bin/bash
# Persist the internal speaker fix for this Hygon/Lenovo platform.
#
# The onboard audio controller is PCI 1d94:14c9 at 0000:06:00.6. Debian's
# snd_hda_intel module exists, but it does not auto-match this PCI ID, so the
# device must be bound explicitly. The resulting codec is Conexant SN6180.
#
# Conflict policy: a managed path whose bytes differ is copied to
# <path>.bak-hygon-hda-<timestamp> before replacement. Backup failure aborts
# with the destination untouched. Identical bytes are left in place.
# Verify failure, system unit failure, or user-manager failure rolls this run
# back. No user bus is not a failure: the user unit stays installed and
# hygon_hda_user_service=SKIP. --uninstall removes only this installer's
# files; it does not restore backups or undo profile/PipeWire edits.

set -euo pipefail

MARKER="# Managed by innogpu-fh2m-debian-trixie install-hygon-hda-audio.sh"
AUDIO_PCI="${HYGON_HDA_AUDIO_PCI:-0000:06:00.6}"
AUDIO_CARD_ID="${HYGON_HDA_CARD_ID:-Intel}"
AUDIO_CARD_NUM="${HYGON_HDA_CARD_NUM:-2}"
TARGET_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
TARGET_HOME="${INNOGPU_X_HOME:-$(getent passwd "$TARGET_USER" 2>/dev/null | cut -d: -f6)}"
TARGET_HOME="${TARGET_HOME:-$HOME}"
RUN_TEST=0
ACTION=install
TEST_ROOT="${INNOGPU_HDA_TEST_ROOT:-}"
PREFIX=""

usage() {
    cat <<USAGE
Usage:
  sudo tools/debian/install-hygon-hda-audio.sh [--test-sound]
  sudo tools/debian/install-hygon-hda-audio.sh --uninstall

Environment overrides:
  HYGON_HDA_AUDIO_PCI   PCI address, default: $AUDIO_PCI
  HYGON_HDA_CARD_ID     ALSA card id after binding, default: $AUDIO_CARD_ID
  HYGON_HDA_CARD_NUM    Fallback ALSA card number for diagnostics, default: $AUDIO_CARD_NUM
  HYGON_HDA_ALSA_CONFIG User ALSA config path, default: ~/.config/alsa/asoundrc
  INNOGPU_X_USER        Target user for ALSA config, default: sudo user/current user
  INNOGPU_X_HOME        Target home for ALSA config
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test-sound) RUN_TEST=1 ;;
        --uninstall) ACTION=uninstall ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ "$ACTION" == uninstall && "$RUN_TEST" == 1 ]]; then
    echo "ERROR: --uninstall does not take --test-sound" >&2
    exit 2
fi

if [[ -n "$TEST_ROOT" ]]; then
    case "$TEST_ROOT" in
        /*) ;;
        *) echo "ERROR: INNOGPU_HDA_TEST_ROOT must be an absolute path != /" >&2; exit 1 ;;
    esac
    NORM="$(readlink -f "$TEST_ROOT" 2>/dev/null || true)"
    if [[ -z "$NORM" || "$NORM" == "/" ]]; then
        echo "ERROR: INNOGPU_HDA_TEST_ROOT must be an absolute path != /" >&2
        exit 1
    fi
    PREFIX="$NORM"
fi

if [[ ${EUID:-$(id -u)} -ne 0 && -z "$PREFIX" ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

if [[ -z "$TARGET_USER" || -z "$TARGET_HOME" || "$TARGET_HOME" == "/" ]]; then
    echo "ERROR: could not determine target user/home; set INNOGPU_X_USER and INNOGPU_X_HOME" >&2
    exit 1
fi
if [[ -n "$PREFIX" ]]; then
    case "$TARGET_HOME" in
        "$PREFIX"/*) ;;
        *) echo "ERROR: INNOGPU_X_HOME must be inside INNOGPU_HDA_TEST_ROOT" >&2; exit 1 ;;
    esac
fi

ALSA_CONFIG="${HYGON_HDA_ALSA_CONFIG:-$TARGET_HOME/.config/alsa/asoundrc}"
ASOUNDRC="$TARGET_HOME/.asoundrc"
SERVICE="$PREFIX/etc/systemd/system/hygon-hda-audio.service"
MODULES_CONF="$PREFIX/etc/modules-load.d/hygon-hda-audio.conf"
USER_BIN="$TARGET_HOME/.local/bin/hygon-hda-audio-user-apply"
USER_SERVICE_DIR="$TARGET_HOME/.config/systemd/user"
USER_SERVICE="$USER_SERVICE_DIR/hygon-hda-audio-user.service"
UNIT_NAME="hygon-hda-audio.service"
USER_UNIT_NAME="hygon-hda-audio-user.service"

declare -A BAK_OF=()
CREATED=()
REPLACED=()
PIPEWIRE_FROM=""
PIPEWIRE_TO=""
SUCCESS=0
ENABLED=0
USER_ENABLED=0
ROLLED=0

user_bus_path() {
    local uid
    uid="$(id -u "$TARGET_USER")"
    if [[ -n "$PREFIX" ]]; then
        printf '%s\n' "$PREFIX/run/user/$uid/bus"
    else
        printf '%s\n' "/run/user/$uid/bus"
    fi
}

user_bus() {
    local bus
    # Fixture only. Production still requires the user manager socket.
    if [[ -n "$PREFIX" && "${INNOGPU_HDA_ASSUME_USER_BUS:-}" == 1 ]]; then
        return 0
    fi
    bus="$(user_bus_path)"
    [[ -S "$bus" ]]
}

run_user_systemctl() {
    if [[ -n "$PREFIX" ]]; then
        systemctl --user "$@"
        return
    fi
    sudo -u "$TARGET_USER" \
        env XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u "$TARGET_USER")/bus" \
        systemctl --user "$@"
}

run_user_wpctl() {
    sudo -u "$TARGET_USER" \
        env XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u "$TARGET_USER")/bus" \
        wpctl "$@" 2>/dev/null
}

rollback() {
    [[ "$ROLLED" -eq 1 ]] && return
    ROLLED=1
    set +e
    if [[ "$USER_ENABLED" -eq 1 ]]; then
        run_user_systemctl disable "$USER_UNIT_NAME" >/dev/null 2>&1
    fi
    if [[ "$ENABLED" -eq 1 ]]; then
        systemctl disable "$UNIT_NAME" >/dev/null 2>&1
    fi
    local dest bak
    for dest in "${CREATED[@]}"; do
        rm -f -- "$dest"
    done
    for dest in "${REPLACED[@]}"; do
        bak="${BAK_OF[$dest]}"
        cp -a -- "$bak" "$dest" || echo "ERROR: restore failed: $dest" >&2
    done
    if [[ -n "$PIPEWIRE_FROM" && -e "$PIPEWIRE_TO" && ! -e "$PIPEWIRE_FROM" ]]; then
        mv -- "$PIPEWIRE_TO" "$PIPEWIRE_FROM" || echo "ERROR: restore failed: $PIPEWIRE_FROM" >&2
    fi
    set -e
}

on_exit() {
    local rc=$?
    [[ "$SUCCESS" -eq 1 ]] || rollback
    exit "$rc"
}

backup_then_replace() {
    local dest="$1" tmp bak
    tmp="$(mktemp "${TMPDIR:-/tmp}/hygon-hda.XXXXXX")"
    cat > "$tmp"
    if [[ -f "$dest" && ! -L "$dest" ]] && cmp -s "$tmp" "$dest"; then
        rm -f -- "$tmp"
        return 0
    fi
    install -d -m 0755 -- "$(dirname "$dest")"
    if [[ -e "$dest" || -L "$dest" ]]; then
        bak="${dest}.bak-hygon-hda-$(date +%Y%m%d-%H%M%S-%N)"
        cp -a -- "$dest" "$bak" || {
            rm -f -- "$tmp"
            echo "ERROR: backup failed: $dest" >&2
            exit 1
        }
        BAK_OF["$dest"]="$bak"
        REPLACED+=("$dest")
        printf 'hygon_hda_backup=%s\n' "$bak"
        rm -f -- "$dest"
    else
        CREATED+=("$dest")
    fi
    install -m "$2" -- "$tmp" "$dest"
    rm -f -- "$tmp"
}

owned_text() {
    local path="$1" legacy="$2"
    [[ -f "$path" && ! -L "$path" ]] || return 1
    grep -Fq "$MARKER" "$path" && return 0
    grep -Fq "$legacy" "$path"
}

refuse_foreign() {
    local path="$1" kind="$2" body
    [[ -e "$path" || -L "$path" ]] || return 0
    case "$kind" in
        system-unit) owned_text "$path" "Bind Hygon HDA audio and enable internal speakers" && return 0 ;;
        user-unit) owned_text "$path" "Select HDA Intel internal speaker output" && return 0 ;;
        modules)
            body="$(grep -v '^#' "$path" | grep -v '^$' || true)"
            [[ "$body" == "snd_hda_intel" ]] && return 0
            ;;
        helper) owned_text "$path" "unset ALSA_CONFIG_PATH" && return 0 ;;
        alsa) owned_text "$path" "User ALSA defaults. Loaded through ~/.asoundrc" && return 0 ;;
        asoundrc) [[ -L "$path" && "$(readlink "$path")" == "$ALSA_CONFIG" ]] && return 0 ;;
    esac
    echo "hygon_hda_uninstall=FAIL reason=foreign:$path" >&2
    exit 1
}

uninstall_audio() {
    local path
    refuse_foreign "$SERVICE" system-unit
    refuse_foreign "$MODULES_CONF" modules
    refuse_foreign "$USER_SERVICE" user-unit
    refuse_foreign "$USER_BIN" helper
    refuse_foreign "$ALSA_CONFIG" alsa
    refuse_foreign "$ASOUNDRC" asoundrc
    if [[ -f "$USER_SERVICE" ]] && user_bus; then
        run_user_systemctl disable "$USER_UNIT_NAME"
        if run_user_systemctl is-active --quiet "$USER_UNIT_NAME"; then
            run_user_systemctl stop "$USER_UNIT_NAME"
        fi
    elif [[ -f "$USER_SERVICE" ]]; then
        echo "hygon_hda_user_service=SKIP reason=no_session"
    fi
    if [[ -f "$SERVICE" ]]; then
        systemctl disable "$UNIT_NAME"
        if systemctl is-active --quiet "$UNIT_NAME"; then
            systemctl stop "$UNIT_NAME"
        fi
    fi
    for kind in "$SERVICE" "$MODULES_CONF" "$USER_SERVICE" "$USER_BIN"; do
        if [[ -e "$kind" || -L "$kind" ]]; then
            rm -f -- "$kind"
            printf 'hygon_hda_removed=%s\n' "$kind"
        fi
    done
    if [[ -L "$ASOUNDRC" ]]; then
        rm -f -- "$ASOUNDRC"
        printf 'hygon_hda_removed=%s\n' "$ASOUNDRC"
    fi
    if [[ -f "$ALSA_CONFIG" ]]; then
        rm -f -- "$ALSA_CONFIG"
        printf 'hygon_hda_removed=%s\n' "$ALSA_CONFIG"
    fi
    systemctl daemon-reload
    if user_bus; then
        run_user_systemctl daemon-reload
    fi
    echo "hygon_hda_uninstall=PASS"
}

if [[ "$ACTION" == uninstall ]]; then
    uninstall_audio
    exit 0
fi

trap on_exit EXIT

disable_global_alsa_config_path() {
    local profile bak
    for profile in "$TARGET_HOME/.profile" "$TARGET_HOME/.zprofile"; do
        [[ -f "$profile" ]] || continue
        if grep -q '^export ALSA_CONFIG_PATH="\$XDG_CONFIG_HOME/alsa/asoundrc"$' "$profile"; then
            bak="${profile}.bak-hygon-hda-$(date +%Y%m%d-%H%M%S-%N)"
            cp -a -- "$profile" "$bak"
            BAK_OF["$profile"]="$bak"
            REPLACED+=("$profile")
            printf 'hygon_hda_backup=%s\n' "$bak"
            perl -0pi -e 's/^export ALSA_CONFIG_PATH="\$XDG_CONFIG_HOME\/alsa\/asoundrc"$/# disabled: ALSA_CONFIG_PATH replaces system alsa.conf and breaks PipeWire\n# export ALSA_CONFIG_PATH="\\$XDG_CONFIG_HOME\/alsa\/asoundrc"/m' "$profile"
        fi
    done
}

disable_legacy_pipewire_user_config() {
    local conf="$TARGET_HOME/.config/pipewire/pipewire.conf" dest
    [[ -f "$conf" ]] || return 0
    if grep -Eq '/usr/bin/(wireplumber|pipewire).*pipewire-pulse|context.exec' "$conf"; then
        dest="$conf.disabled-$(date +%Y%m%d-%H%M%S-%N)"
        mv -- "$conf" "$dest"
        PIPEWIRE_FROM="$conf"
        PIPEWIRE_TO="$dest"
        echo "Disabled legacy PipeWire override: $conf"
    fi
}

verify_units() {
    systemd-analyze verify "$SERVICE"
    # shortcut: no user bus means file-level verify only; --user needs a manager.
    if user_bus; then
        if [[ -n "$PREFIX" ]]; then
            systemd-analyze --user verify "$USER_SERVICE"
        else
            sudo -u "$TARGET_USER" \
                env XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
                    DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u "$TARGET_USER")/bus" \
                systemd-analyze --user verify "$USER_SERVICE"
        fi
    else
        systemd-analyze verify "$USER_SERVICE"
    fi
    echo "hygon_hda_verify=PASS"
}

install -d -m 0755 -- "$PREFIX/etc/modules-load.d" "$PREFIX/etc/systemd/system"
backup_then_replace "$MODULES_CONF" 0644 <<EOF
$MARKER
snd_hda_intel
EOF

backup_then_replace "$SERVICE" 0644 <<EOF
$MARKER
[Unit]
Description=Bind Hygon HDA audio and enable internal speakers
After=systemd-modules-load.service systemd-udevd.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/sbin/modprobe snd_hda_intel
ExecStart=/bin/sh -c 'dev=/sys/bus/pci/devices/$AUDIO_PCI; [ -e "\$dev" ] || exit 0; echo snd_hda_intel > "\$dev/driver_override"; [ -L "\$dev/driver" ] || echo $AUDIO_PCI > /sys/bus/pci/drivers_probe'
ExecStart=/usr/bin/udevadm trigger --subsystem-match=sound
ExecStart=/usr/bin/udevadm settle
ExecStart=/bin/sh -c '/usr/bin/amixer -c $AUDIO_CARD_ID sset "Auto-Mute Mode" Disabled || true'
ExecStart=/bin/sh -c '/usr/bin/amixer -c $AUDIO_CARD_ID sset Master 80%% unmute || true'
ExecStart=/bin/sh -c '/usr/bin/amixer -c $AUDIO_CARD_ID sset Speaker 100%% unmute || true'
ExecStart=/bin/sh -c '/usr/sbin/alsactl store $AUDIO_CARD_ID || true'

[Install]
WantedBy=multi-user.target
EOF

backup_then_replace "$ALSA_CONFIG" 0644 <<EOF
$MARKER
# User ALSA defaults. Loaded through ~/.asoundrc, not ALSA_CONFIG_PATH.
# Keep this minimal so PipeWire/WirePlumber can still use the system ALSA config.

pcm.!default {
        type plug
        slave.pcm "hw:$AUDIO_CARD_ID,0"
}

ctl.!default {
        type hw
        card "$AUDIO_CARD_ID"
}
EOF
chown "$TARGET_USER:$TARGET_USER" "$ALSA_CONFIG" 2>/dev/null || true

if [[ -L "$ASOUNDRC" && "$(readlink "$ASOUNDRC")" == "$ALSA_CONFIG" ]]; then
    :
else
    if [[ -e "$ASOUNDRC" || -L "$ASOUNDRC" ]]; then
        bak="${ASOUNDRC}.bak-hygon-hda-$(date +%Y%m%d-%H%M%S-%N)"
        cp -a -- "$ASOUNDRC" "$bak" || { echo "ERROR: backup failed: $ASOUNDRC" >&2; exit 1; }
        BAK_OF["$ASOUNDRC"]="$bak"
        REPLACED+=("$ASOUNDRC")
        printf 'hygon_hda_backup=%s\n' "$bak"
        rm -f -- "$ASOUNDRC"
    else
        CREATED+=("$ASOUNDRC")
    fi
    ln -sfn "$ALSA_CONFIG" "$ASOUNDRC"
fi
chown -h "$TARGET_USER:$TARGET_USER" "$ASOUNDRC" 2>/dev/null || true

disable_global_alsa_config_path
disable_legacy_pipewire_user_config

backup_then_replace "$USER_BIN" 0755 <<EOF
#!/bin/sh
$MARKER
unset ALSA_CONFIG_PATH

/usr/bin/amixer -c "$AUDIO_CARD_ID" sset "Auto-Mute Mode" Disabled >/dev/null 2>&1 || true
/usr/bin/amixer -c "$AUDIO_CARD_ID" sset Master 80% unmute >/dev/null 2>&1 || true
/usr/bin/amixer -c "$AUDIO_CARD_ID" sset Speaker 100% unmute >/dev/null 2>&1 || true
/usr/sbin/alsactl store "$AUDIO_CARD_ID" >/dev/null 2>&1 || true

if command -v wpctl >/dev/null 2>&1; then
    sink_id="\$(wpctl status | awk '/HDA Intel/ && /\\[vol:/ { for (i = 1; i <= NF; i++) if (\$i ~ /^[0-9]+\\.\$/) { sub(/\\./, "", \$i); print \$i; exit } }')"
    if [ -n "\$sink_id" ]; then
        wpctl set-default "\$sink_id" >/dev/null 2>&1 || true
        wpctl set-volume "\$sink_id" 0.80 >/dev/null 2>&1 || true
        wpctl set-mute "\$sink_id" 0 >/dev/null 2>&1 || true
    fi
fi
EOF
chown "$TARGET_USER:$TARGET_USER" "$USER_BIN" 2>/dev/null || true

backup_then_replace "$USER_SERVICE" 0644 <<EOF
$MARKER
[Unit]
Description=Select HDA Intel internal speaker output
After=pipewire.service pipewire-pulse.service wireplumber.service
Wants=pipewire.service pipewire-pulse.service wireplumber.service

[Service]
Type=oneshot
Environment=ALSA_CONFIG_PATH=
ExecStart=$USER_BIN

[Install]
WantedBy=default.target
EOF
chown "$TARGET_USER:$TARGET_USER" "$USER_SERVICE" 2>/dev/null || true

verify_units
systemctl daemon-reload
systemctl enable "$UNIT_NAME"
ENABLED=1
systemctl restart "$UNIT_NAME"

if user_bus; then
    run_user_systemctl daemon-reload
    run_user_systemctl enable "$USER_UNIT_NAME"
    USER_ENABLED=1
    run_user_systemctl restart "$USER_UNIT_NAME"
    echo "hygon_hda_user_service=PASS"
else
    echo "hygon_hda_user_service=SKIP reason=no_session"
fi

if [[ -z "$PREFIX" ]]; then
    uid="$(id -u "$TARGET_USER")"
    if [[ -S "/run/user/$uid/bus" ]]; then
        run_user_systemctl unset-environment ALSA_CONFIG_PATH || true
        run_user_systemctl restart pipewire.service pipewire-pulse.service wireplumber.service || true
        sleep 2
    fi
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset "Auto-Mute Mode" Disabled >/dev/null 2>&1 || true
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset Master 80% unmute >/dev/null 2>&1 || true
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset Speaker 100% unmute >/dev/null 2>&1 || true
    /usr/sbin/alsactl store "$AUDIO_CARD_ID" >/dev/null 2>&1 || true
    if command -v wpctl >/dev/null 2>&1; then
        sink_id="$(
            run_user_wpctl status |
                awk '/HDA Intel/ && /\[vol:/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9]+\.$/) { sub(/\./, "", $i); print $i; exit } }'
        )" || true
        if [[ -n "${sink_id:-}" ]]; then
            run_user_wpctl set-default "$sink_id" || true
            run_user_wpctl set-volume "$sink_id" 0.80 || true
            run_user_wpctl set-mute "$sink_id" 0 || true
        fi
    fi
    echo
    echo "===== HDA PCI Binding ====="
    lspci -nnk -s "$AUDIO_PCI" || true
    echo
    echo "===== ALSA Cards ====="
    sudo -u "$TARGET_USER" env -u ALSA_CONFIG_PATH HOME="$TARGET_HOME" aplay -l || true
    echo
    echo "===== Mixer ====="
    sudo -u "$TARGET_USER" env -u ALSA_CONFIG_PATH HOME="$TARGET_HOME" amixer -c "$AUDIO_CARD_ID" sget Master || true
    sudo -u "$TARGET_USER" env -u ALSA_CONFIG_PATH HOME="$TARGET_HOME" amixer -c "$AUDIO_CARD_ID" sget Speaker || true
    echo
    echo "===== PipeWire ====="
    run_user_wpctl status || true
    if [[ "$RUN_TEST" == "1" ]]; then
        echo
        echo "===== Speaker Test ====="
        sudo -u "$TARGET_USER" env -u ALSA_CONFIG_PATH HOME="$TARGET_HOME" speaker-test -D default -c 2 -t sine -f 440 -l 1
    fi
fi

echo
echo "Installed:"
echo "  $SERVICE"
echo "  $MODULES_CONF"
echo "  $ALSA_CONFIG"
echo "  $ASOUNDRC -> $ALSA_CONFIG"
echo "  $USER_SERVICE"
echo "  $USER_BIN"
echo "hygon_hda_install=PASS"
SUCCESS=1
