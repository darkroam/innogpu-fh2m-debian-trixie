# Copyright 2026 the fantgpu maintainers. Glue is GPL-3.0-or-later.
# The patch bytes are kernel changes and stay GPL-2.0-only.

EAPI=8

DESCRIPTION="Machine patches for gentoo-sources and gentoo-kernel user-patches"
HOMEPAGE="https://github.com/timhant/innogpu-fh2m-debian-trixie"
SRC_URI=""
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64"
RESTRICT="bindist"
S="${WORKDIR}"

src_install() {
	local src="${FILESDIR}/../../patches"
	insinto /usr/share/fantgpu-kernel-patches
	doins \
		"${src}/001-hygon-148c-xhci-reset-on-resume.patch" \
		"${src}/002-kaitian-x7h-battery-notification-delay.patch" \
		"${src}/003-pinctrl-amd-hygon-gpio.patch" \
		"${src}/004-snd-hda-hygon-14c9.patch"
	exeinto /usr/libexec/fantgpu
	doexe "${FILESDIR}/install-patches.sh"
}

pkg_postinst() {
	local root=${EROOT%/}
	PATCH_SRC="${root}/usr/share/fantgpu-kernel-patches" \
		DESTDIR="${root}" \
		"${root}/usr/libexec/fantgpu/install-patches.sh" \
		|| die "kernel patch copy failed"
	einfo "补丁已落位，重编/升级内核生效后再装驱动"
}

pkg_prerm() {
	local root=${EROOT%/}
	PATCH_REMOVE=1 PATCH_SRC="${root}/usr/share/fantgpu-kernel-patches" \
		DESTDIR="${root}" \
		"${root}/usr/libexec/fantgpu/install-patches.sh" \
		|| die "kernel patch removal failed"
}
