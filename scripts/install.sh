#!/bin/bash
# ============================================================
# innogpu-fh2m Debian Trixie (kernel 6.12) Installation Script
# ============================================================
# This script patches and installs the Innosilicon Fantasy II-M
# (innogpu fh2m) GPU driver on Debian Trixie with kernel 6.12.
#
# Prerequisites:
#   - Debian Trixie (13) with kernel 6.12.x
#   - Innosilicon official driver package:
#     innogpu-fh2m_3.3.3.42-driver-linux-desktop-sp-generic_amd64.deb
#   - Build tools: dkms, build-essential, linux-headers
#
# Usage:
#   sudo ./install.sh /path/to/innogpu-fh2m_3.3.3.42-*.deb
# ============================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()   { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }

# --- Checks ---
[[ $EUID -ne 0 ]] && error "Please run as root (sudo)"
[[ -z "$1" ]] && error "Usage: $0 /path/to/innogpu-fh2m_3.3.3.42-*.deb"
[[ ! -f "$1" ]] && error "File not found: $1"

DEB_PATH="$(realpath "$1")"
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PATCH_FILE="$SCRIPT_DIR/patches/001-kernel-6.12-compat.patch"
KERNEL_VER="$(uname -r)"

[[ ! -f "$PATCH_FILE" ]] && error "Patch file not found: $PATCH_FILE"

log "Kernel version: $KERNEL_VER"
log "Driver package: $DEB_PATH"

# --- Check kernel version ---
KMAJOR=$(echo "$KERNEL_VER" | cut -d. -f1)
KMINOR=$(echo "$KERNEL_VER" | cut -d. -f2)
if [[ "$KMAJOR" -lt 6 ]] || { [[ "$KMAJOR" -eq 6 ]] && [[ "$KMINOR" -lt 9 ]]; }; then
    warn "Kernel $KERNEL_VER may not need these patches (designed for 6.9+)"
    read -p "Continue anyway? [y/N] " -n 1 -r
    echo
    [[ ! $REPLY =~ ^[Yy]$ ]] && exit 0
fi

# --- Install dependencies ---
log "Installing build dependencies..."
apt-get install -y dkms build-essential linux-headers-"$KERNEL_VER" 2>/dev/null || \
    warn "Some packages may already be installed"

# --- Extract and install official driver ---
log "Installing official driver package (this extracts source + firmware)..."
dpkg -i "$DEB_PATH" 2>/dev/null || apt-get install -f -y 2>/dev/null || true

# --- Verify DKMS source exists ---
DKMS_SRC="/usr/src/innogpu-kernel-2.2"
[[ ! -d "$DKMS_SRC" ]] && error "DKMS source not found at $DKMS_SRC — driver package may have failed to extract"

# --- Apply patches ---
log "Applying kernel 6.12 compatibility patches..."
cd "$DKMS_SRC"

# Try to apply; if already applied, skip
if patch -p3 --dry-run -N < "$PATCH_FILE" > /dev/null 2>&1; then
    patch -p3 -N < "$PATCH_FILE"
    log "Patches applied successfully"
elif patch -p3 --dry-run -R < "$PATCH_FILE" > /dev/null 2>&1; then
    log "Patches already applied, skipping"
else
    warn "Patch may not apply cleanly — attempting with fuzz..."
    patch -p3 -N --fuzz=3 < "$PATCH_FILE" || error "Failed to apply patches"
fi

# --- Build with DKMS ---
log "Building kernel module with DKMS..."
dkms build innogpu-kernel/2.2 -k "$KERNEL_VER" --force 2>&1 | tail -5

# --- Install ---
log "Installing kernel module..."
dkms install innogpu-kernel/2.2 -k "$KERNEL_VER" --force 2>&1 | tail -5

# --- Post-install configuration ---
log "Configuring system..."

# Disable Wayland in GDM (innogpu doesn't support it well yet)
if [[ -f /etc/gdm3/daemon.conf ]]; then
    sed -i 's/#WaylandEnable=false/WaylandEnable=false/' /etc/gdm3/daemon.conf
    grep -q 'WaylandEnable=false' /etc/gdm3/daemon.conf || \
        sed -i '/\[daemon\]/a WaylandEnable=false' /etc/gdm3/daemon.conf
    log "Disabled Wayland in GDM"
fi

# Mask sw-inno-gl.service (official package service that recreates 0-innogpu.conf on every boot)
if [[ -f /lib/systemd/system/sw-inno-gl.service ]] || \
   [[ -L /etc/systemd/system/sw-inno-gl.service ]] || \
   [[ -f /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service ]]; then
    rm -f /etc/systemd/system/sw-inno-gl.service
    rm -f /etc/systemd/system/multi-user.target.wants/sw-inno-gl.service
    ln -sf /dev/null /etc/systemd/system/sw-inno-gl.service
    systemctl daemon-reload 2>/dev/null || true
    log "Masked sw-inno-gl.service (prevents 0-innogpu.conf recreation on boot)"
fi

# Remove innogpu ldconfig override (userspace libs may not be compatible)
if [[ -f /etc/ld.so.conf.d/0-innogpu.conf ]]; then
    mv -f /etc/ld.so.conf.d/0-innogpu.conf /etc/ld.so.conf.d/0-innogpu.conf.disabled
    ldconfig
    log "Disabled innogpu ldconfig override"
fi

# Rename incompatible DRI driver if present
if [[ -f /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so ]]; then
    mv /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so \
       /usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak
    log "Disabled incompatible innogpu DRI driver (mesa version mismatch)"
fi

# Create xorg.conf for modesetting driver
cat > /etc/X11/xorg.conf << 'XORG'
Section "Device"
    Identifier "innogpu"
    Driver "modesetting"
    Option "kmsdev" "/dev/dri/card0"
EndSection
XORG
log "Created /etc/X11/xorg.conf for modesetting driver"

# Disable Xvfb/x11vnc if they conflict
for svc in xvfb x11vnc novnc; do
    if systemctl is-enabled "$svc" 2>/dev/null | grep -q enabled; then
        systemctl disable "$svc" 2>/dev/null
        log "Disabled conflicting service: $svc"
    fi
done

# --- Done ---
echo ""
log "============================================"
log "  Installation complete!"
log "============================================"
log ""
log "  Please reboot to load the new driver."
log ""
log "  After reboot, verify with:"
log "    dmesg | grep innogpu"
log "    ls /dev/dri/card0"
log ""
log "  NOTE: 3D acceleration requires compatible"
log "  userspace libraries from Innosilicon."
log "  Currently using modesetting (2D + software 3D)."
log "============================================"
