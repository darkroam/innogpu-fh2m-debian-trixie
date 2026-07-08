#!/bin/bash
# Install a small boot-time repair service for systems where innogpu registers
# DRM/fbdev minors in sysfs but devtmpfs/udev misses the /dev nodes.

set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

SERVICE=/etc/systemd/system/innogpu-repair-dri-nodes.service
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"

if ! command -v innogpu-repair-dri-nodes >/dev/null 2>&1; then
    if [[ -x /usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh ]]; then
        ln -sf ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh /usr/sbin/innogpu-repair-dri-nodes
        ln -sf ../share/innogpu-fh2m-trixie/repair-dri-nodes.sh /usr/bin/innogpu-repair-dri-nodes
    elif [[ -x "$ROOT/scripts/repair-dri-nodes.sh" ]]; then
        install -d -m 0755 /usr/local/sbin
        ln -sf "$ROOT/scripts/repair-dri-nodes.sh" /usr/local/sbin/innogpu-repair-dri-nodes
    else
        echo "ERROR: innogpu-repair-dri-nodes helper is missing" >&2
        exit 1
    fi
fi

cat > "$SERVICE" <<'EOF'
[Unit]
Description=Repair Innogpu DRM/fbdev device nodes
Documentation=file:/usr/share/innogpu-fh2m-trixie/repair-dri-nodes.sh
DefaultDependencies=no
After=systemd-modules-load.service systemd-udev-settle.service
Before=getty@tty1.service display-manager.service
ConditionPathExists=/sys/module/innogpu

[Service]
Type=oneshot
ExecStart=/usr/sbin/innogpu-repair-dri-nodes
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable innogpu-repair-dri-nodes.service
systemctl start innogpu-repair-dri-nodes.service || true

echo
echo "Installed boot-time DRM/fbdev node repair service:"
systemctl --no-pager --full status innogpu-repair-dri-nodes.service || true
echo
echo "Current nodes:"
ls -l /dev/dri /dev/dri/by-path /dev/fb0 2>/dev/null || true
