#!/bin/bash
# Install Debian build dependencies for the pinned Picom source.

set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
    echo "ERROR: run as root: sudo $0" >&2
    exit 1
fi

apt-get update
apt-get install -y \
    build-essential \
    git \
    meson \
    ninja-build \
    pkg-config \
    procps \
    util-linux \
    libconfig-dev \
    libdbus-1-dev \
    libegl-dev \
    libev-dev \
    libgl-dev \
    libepoxy-dev \
    libpcre2-dev \
    libpixman-1-dev \
    libx11-xcb-dev \
    libxcb1-dev \
    libxcb-composite0-dev \
    libxcb-damage0-dev \
    libxcb-glx0-dev \
    libxcb-image0-dev \
    libxcb-present-dev \
    libxcb-randr0-dev \
    libxcb-render0-dev \
    libxcb-render-util0-dev \
    libxcb-shape0-dev \
    libxcb-util-dev \
    libxcb-xfixes0-dev \
    uthash-dev \
    xcompmgr
