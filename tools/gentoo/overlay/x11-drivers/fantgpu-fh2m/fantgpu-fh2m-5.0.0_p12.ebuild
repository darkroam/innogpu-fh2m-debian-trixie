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
# shortcut: IUSE=-wayland omits dev-libs/wayland, but the amd64 blobs still NEEDED libwayland-client.so.0 and libwayland-server.so.0. Drop the flag when those NEEDED lines go away.
IUSE="+wayland"
DEPEND="virtual/linux-sources"
RDEPEND="
	>=x11-base/xorg-server-21.1.20:0 <x11-base/xorg-server-22:0
	media-libs/libva:0/2
	media-libs/libglvnd
	media-libs/libepoxy
	x11-libs/libdrm
	x11-libs/libX11
	x11-libs/libXext
	x11-libs/libXfixes
	x11-libs/libXxf86vm
	x11-libs/libxcb
	x11-libs/libxshmfence
	x11-libs/pixman
	dev-libs/expat
	sys-libs/zlib
	virtual/libudev
	sys-libs/glibc
	sys-devel/gcc
	wayland? ( dev-libs/wayland )
"
S="${WORKDIR}"

repo_root() {
	local overlay
	overlay=$(dirname "$(dirname "$(dirname "${EBUILD}")")")
	if [[ -x ${overlay}/../build-driver.sh ]]; then
		dirname "$(dirname "$(dirname "${overlay}")")"
		return
	fi
	[[ -f ${overlay}/repo-root ]] || die "repo root not found"
	cat "${overlay}/repo-root"
}

pkg_setup() {
	[[ -d /lib/modules/$(uname -r)/build ]] || die "kernel build directory missing"
	mkdir -p /var/lib/fantgpu/rollback || die "rollback dir"
}

src_compile() {
	local root
	root=$(repo_root)
	addread "${root}"
	env -u ARCH KERNEL="$(uname -r)" GENTOO_WORK="${T}/gentoo-fh2m" "${root}/tools/gentoo/build-driver.sh" || die "build failed"
}

src_install() {
	local root
	root=$(repo_root)
	addread "${root}"
	addwrite /var/lib/fantgpu
	env -u ARCH DESTDIR="${D}" KERNEL="$(uname -r)" GENTOO_WORK="${T}/gentoo-fh2m" \
		GENTOO_ROLLBACK=/var/lib/fantgpu/rollback \
		"${root}/tools/gentoo/install-driver.sh" || die "install failed"
}

pkg_postinst() {
	depmod -a "$(uname -r)" || ewarn "depmod failed"
	ldconfig || ewarn "ldconfig failed"
	einfo "模块已写入磁盘。不要热卸载。重启后才是正在运行的那份。"
}

pkg_prerm() {
	if grep -q '^fantgpu ' /proc/modules 2>/dev/null; then
		ewarn "模块已加载。不要热卸载。先重启，再把这次卸载当成完成。"
	fi
}

pkg_postrm() {
	depmod -a "$(uname -r)" || ewarn "depmod failed"
	ldconfig || ewarn "ldconfig failed"
}
