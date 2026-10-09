#!/bin/bash
# Install Debian packages required to build/load the DKMS module and validate Xorg/GL.

set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

kernel="$(uname -r)"

apt-get update
apt-get install -y \
    build-essential \
    dkms \
    linux-headers-amd64 \
    kmod \
    initramfs-tools \
    xserver-xorg-core \
    xinit \
    x11-xserver-utils \
    x11-utils \
    arandr \
    bc \
    dmenu \
    util-linux \
    mesa-utils \
    libgl1 \
    libegl1 \
    libgbm1 \
    alsa-utils \
    pipewire \
    pipewire-pulse \
    wireplumber \
    dwm

if apt-cache show "linux-headers-$kernel" >/dev/null 2>&1; then
    apt-get install -y "linux-headers-$kernel"
else
    echo "WARN: exact linux-headers-$kernel package not found; linux-headers-amd64 was installed instead." >&2
fi
