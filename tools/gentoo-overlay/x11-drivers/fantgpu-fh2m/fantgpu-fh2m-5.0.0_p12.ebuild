# Copyright 2026 the fantgpu maintainers. Glue is GPL-3.0-or-later.
# Userspace and firmware stay the vendor payload and are not redistributed.

EAPI=8

DESCRIPTION="Fantasy II-M fantgpu module, firmware, and userspace"
HOMEPAGE="https://github.com/timhant/innogpu-fh2m-debian-trixie"
SRC_URI=""
LICENSE="GPL-3 fantgpu-vendor"
SLOT="0"
KEYWORDS="~amd64"
RESTRICT="bindist mirror strip"
# Slot on this machine is 0/21.1.25, so :0/21.1 would not match. Pin the 21.1 ABI by version.
DEPEND="virtual/linux-sources"
RDEPEND="
	>=x11-base/xorg-server-21.1.20:0 <x11-base/xorg-server-22:0
	media-libs/libva:0/2
	x11-libs/libdrm
"
S="${WORKDIR}"

repo_root() {
	local overlay
	overlay=$(portageq get_repo_path "${EROOT:-/}" fantgpu)
	dirname "$(dirname "${overlay}")"
}

pkg_setup() {
	[[ -d /lib/modules/$(uname -r)/build ]] || die "kernel build directory missing"
}

src_compile() {
	local root
	root=$(repo_root)
	GENTOO_WORK="${T}/gentoo-fh2m" "${root}/tools/gentoo-fh2m.sh" build
}

src_install() {
	local root
	root=$(repo_root)
	DESTDIR="${D}" GENTOO_WORK="${T}/gentoo-fh2m" "${root}/tools/gentoo-fh2m.sh" install
}
