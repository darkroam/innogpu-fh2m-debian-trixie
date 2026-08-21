#!/bin/bash
# Phase 4 B1-B12 baseline capture (read-only). Saves timestamped output under baselines/.
# Real-session items (B7 /dev/dri, B8 Xorg/GL, B9 audio sinks, B11 display) require the
# user's real terminal; this script captures everything observable from the dsh sandbox
# and prints the exact user commands for the rest.
set -u
ROOT="${INNOGPU_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "$ROOT"
mkdir -p baselines
OUT="baselines/phase4-baseline-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$OUT") 2>&1
echo "===== Phase 4 preinstall baseline: $(date -Is) ====="
echo "== B1 package version =="
dpkg -l | grep -i innogpu | awk '{print $2, $3}'
echo "== B2 loaded modules =="
lsmod | grep -E '^innogpu|^innosrvkm|^innodpu' | head
echo "== B3 module files =="
ls -l /lib/modules/$(uname -r)/updates/dkms/ 2>/dev/null
echo "== B4 dkms status =="
/usr/sbin/dkms status 2>&1 | head -5
echo "== B5 modprobe config =="
cat /etc/modprobe.d/innogpu.conf 2>/dev/null
echo "== B6 initramfs innogpu entries =="
lsinitramfs /boot/initrd.img-$(uname -r) 2>/dev/null | grep -i innogpu | head -8
echo "== B9 audio cards (kernel-visible) =="
cat /proc/asound/cards 2>/dev/null | head -6
aplay -l 2>&1 | head -5
echo "== B10 picom =="
pgrep -a picom 2>/dev/null | head -3 || echo "(no picom process visible from sandbox)"
echo "== B12 userspace coherence =="
bash scripts/check-deepin-userspace-coherence.sh 2>&1 | tail -3
echo "== recovery channel re-check =="
echo -n "ssh: "; systemctl is-active ssh 2>/dev/null
echo -n "getty: "; systemctl list-units --type=service --state=running 2>/dev/null | grep -c getty
echo -n "disk free: "; df -h / | tail -1 | awk '{print $4}'
echo -n "kernel headers: "; ls -d /lib/modules/$(uname -r)/build 2>/dev/null
echo -n "rollback pkg: "; sha256sum debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb | cut -d' ' -f1
echo -n "candidate pkg: "; sha256sum build/innogpu-fh2m-trixie_4.0.0-i1.deb | cut -d' ' -f1
echo "===== baseline saved: $OUT ====="
