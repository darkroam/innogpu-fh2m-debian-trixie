#!/bin/bash
# Persist the internal speaker fix for this Hygon/Lenovo platform.
#
# The onboard audio controller is PCI 1d94:14c9 at 0000:06:00.6. Debian's
# snd_hda_intel module exists, but it does not auto-match this PCI ID, so the
# device must be bound explicitly. The resulting codec is Conexant SN6180.

set -euo pipefail

AUDIO_PCI="${HYGON_HDA_AUDIO_PCI:-0000:06:00.6}"
AUDIO_CARD_ID="${HYGON_HDA_CARD_ID:-Intel}"
AUDIO_CARD_NUM="${HYGON_HDA_CARD_NUM:-2}"
TARGET_USER="${INNOGPU_X_USER:-${SUDO_USER:-${USER:-}}}"
TARGET_HOME="${INNOGPU_X_HOME:-$(getent passwd "$TARGET_USER" 2>/dev/null | cut -d: -f6)}"
TARGET_HOME="${TARGET_HOME:-$HOME}"
ALSA_CONFIG="${HYGON_HDA_ALSA_CONFIG:-$TARGET_HOME/.config/alsa/asoundrc}"
ASOUNDRC="$TARGET_HOME/.asoundrc"
SERVICE=/etc/systemd/system/hygon-hda-audio.service
MODULES_CONF=/etc/modules-load.d/hygon-hda-audio.conf
USER_BIN="$TARGET_HOME/.local/bin/hygon-hda-audio-user-apply"
USER_SERVICE_DIR="$TARGET_HOME/.config/systemd/user"
USER_SERVICE="$USER_SERVICE_DIR/hygon-hda-audio-user.service"
RUN_TEST=0

usage() {
    cat <<USAGE
Usage:
  sudo scripts/install-hygon-hda-audio.sh [--test-sound]

Environment overrides:
  HYGON_HDA_AUDIO_PCI   PCI address, default: $AUDIO_PCI
  HYGON_HDA_CARD_ID     ALSA card id after binding, default: $AUDIO_CARD_ID
  HYGON_HDA_CARD_NUM    Fallback ALSA card number for diagnostics, default: $AUDIO_CARD_NUM
  HYGON_HDA_ALSA_CONFIG User ALSA config path, default: $ALSA_CONFIG
  INNOGPU_X_USER        Target user for ALSA config, default: sudo user/current user
  INNOGPU_X_HOME        Target home for ALSA config
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test-sound) RUN_TEST=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "ERROR: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

if [[ -z "$TARGET_USER" || -z "$TARGET_HOME" ]]; then
    echo "ERROR: could not determine target user/home; set INNOGPU_X_USER and INNOGPU_X_HOME" >&2
    exit 1
fi

disable_global_alsa_config_path() {
    local profile
    for profile in "$TARGET_HOME/.profile" "$TARGET_HOME/.zprofile"; do
        [[ -f "$profile" ]] || continue
        if grep -q '^export ALSA_CONFIG_PATH="\$XDG_CONFIG_HOME/alsa/asoundrc"$' "$profile"; then
            cp -a "$profile" "$profile.bak-hygon-hda-$(date +%Y%m%d-%H%M%S)"
            perl -0pi -e 's/^export ALSA_CONFIG_PATH="\$XDG_CONFIG_HOME\/alsa\/asoundrc"$/# disabled: ALSA_CONFIG_PATH replaces system alsa.conf and breaks PipeWire\n# export ALSA_CONFIG_PATH="\\$XDG_CONFIG_HOME\/alsa\/asoundrc"/m' "$profile"
        fi
    done
}

disable_legacy_pipewire_user_config() {
    local conf="$TARGET_HOME/.config/pipewire/pipewire.conf"
    [[ -f "$conf" ]] || return 0

    if grep -Eq '/usr/bin/(wireplumber|pipewire).*pipewire-pulse|context.exec' "$conf"; then
        mv "$conf" "$conf.disabled-$(date +%Y%m%d-%H%M%S)"
        echo "Disabled legacy PipeWire override: $conf"
    fi
}

run_user_systemctl() {
    sudo -u "$TARGET_USER" \
        env XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u "$TARGET_USER")/bus" \
        systemctl --user "$@" 2>/dev/null
}

run_user_wpctl() {
    sudo -u "$TARGET_USER" \
        env XDG_RUNTIME_DIR="/run/user/$(id -u "$TARGET_USER")" \
            DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u "$TARGET_USER")/bus" \
        wpctl "$@" 2>/dev/null
}

restart_user_audio_if_running() {
    local uid
    uid="$(id -u "$TARGET_USER")"
    [[ -S "/run/user/$uid/bus" ]] || return 0

    run_user_systemctl unset-environment ALSA_CONFIG_PATH || true
    run_user_systemctl restart pipewire.service pipewire-pulse.service wireplumber.service || true
    sleep 2
}

select_pipewire_hda_sink() {
    local sink_id
    if ! command -v wpctl >/dev/null 2>&1; then
        return 0
    fi

    sink_id="$(
        run_user_wpctl status |
            awk '/HDA Intel/ && /\[vol:/ { for (i = 1; i <= NF; i++) if ($i ~ /^[0-9]+\.$/) { sub(/\./, "", $i); print $i; exit } }'
    )"
    [[ -n "$sink_id" ]] || return 0

    run_user_wpctl set-default "$sink_id" || true
    run_user_wpctl set-volume "$sink_id" 0.80 || true
    run_user_wpctl set-mute "$sink_id" 0 || true
}

enable_hda_mixer() {
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset "Auto-Mute Mode" Disabled >/dev/null 2>&1 || true
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset Master 80% unmute >/dev/null 2>&1 || true
    /usr/bin/amixer -c "$AUDIO_CARD_ID" sset Speaker 100% unmute >/dev/null 2>&1 || true
    /usr/sbin/alsactl store "$AUDIO_CARD_ID" >/dev/null 2>&1 || true
}

install_user_audio_service() {
    install -d -m 0755 "$(dirname "$USER_BIN")" "$USER_SERVICE_DIR"

    cat > "$USER_BIN" <<EOF
#!/bin/sh
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

    chmod 0755 "$USER_BIN"
    chown "$TARGET_USER:$TARGET_USER" "$USER_BIN" 2>/dev/null || true

    cat > "$USER_SERVICE" <<EOF
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
}

install -d -m 0755 /etc/modules-load.d /etc/systemd/system
printf '%s\n' snd_hda_intel > "$MODULES_CONF"

cat > "$SERVICE" <<EOF
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

install -d -m 0755 "$(dirname "$ALSA_CONFIG")"
if [[ -f "$ALSA_CONFIG" ]]; then
    cp -a "$ALSA_CONFIG" "$ALSA_CONFIG.bak-$(date +%Y%m%d-%H%M%S)"
fi

cat > "$ALSA_CONFIG" <<EOF
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
ln -sfn "$ALSA_CONFIG" "$ASOUNDRC"
chown -h "$TARGET_USER:$TARGET_USER" "$ASOUNDRC" 2>/dev/null || true

disable_global_alsa_config_path
disable_legacy_pipewire_user_config
install_user_audio_service

systemctl daemon-reload
systemctl enable hygon-hda-audio.service
systemctl restart hygon-hda-audio.service
restart_user_audio_if_running
enable_hda_mixer
select_pipewire_hda_sink
run_user_systemctl daemon-reload || true
run_user_systemctl enable hygon-hda-audio-user.service || true
run_user_systemctl restart hygon-hda-audio-user.service || true

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

echo
echo "Installed:"
echo "  $SERVICE"
echo "  $MODULES_CONF"
echo "  $ALSA_CONFIG"
echo "  $ASOUNDRC -> $ALSA_CONFIG"
echo "  $USER_SERVICE"
echo "  $USER_BIN"
