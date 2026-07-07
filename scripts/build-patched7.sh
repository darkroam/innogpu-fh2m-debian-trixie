#!/bin/bash
set -euo pipefail
cd /home/ok/src/innogpu-fh2m-debian-trixie
W=$(mktemp -d /tmp/innogpu-pkg7.XXXXXX)
mkdir -p "$W/root" "$W/DEBIAN"
dpkg-deb -x innogpu-fh2m-trixie_3.3.3.42-patched-6.deb "$W/root"
dpkg-deb -e innogpu-fh2m-trixie_3.3.3.42-patched-6.deb "$W/DEBIAN"

install -d "$W/root/usr/share/innogpu-fh2m-trixie"
install -m 0755 scripts/patch-skip-first-gpupll.sh "$W/root/usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh"
install -m 0755 scripts/disable-incompatible-userspace.sh "$W/root/usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh"
install -m 0755 scripts/install-kylin-userspace.sh "$W/root/usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh"
install -m 0755 scripts/test-xorg-once.sh "$W/root/usr/share/innogpu-fh2m-trixie/test-xorg-once.sh"

install -d "$W/root/usr/bin" "$W/root/usr/sbin"
ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/bin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/sbin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/bin/innogpu-disable-incompatible-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/sbin/innogpu-disable-incompatible-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh "$W/root/usr/bin/innogpu-install-kylin-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh "$W/root/usr/sbin/innogpu-install-kylin-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/test-xorg-once.sh "$W/root/usr/bin/innogpu-test-xorg-once"
ln -sfn ../share/innogpu-fh2m-trixie/test-xorg-once.sh "$W/root/usr/sbin/innogpu-test-xorg-once"

python3 - "$W/DEBIAN/control" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
s=s.replace('Version: 3.3.3.42-patched-6','Version: 3.3.3.42-patched-7')
s=s.replace('compatibility fixes, built-in G0M PLL workaround, and Mesa-safe userspace cleanup.', 'compatibility fixes, built-in G0M PLL workaround, safe modesetting fallback, and an experimental Kylin userspace GL installer.')
p.write_text(s)
PY

python3 - "$W/DEBIAN/postinst" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
s=s.replace('innogpu-fh2m-trixie postinst (patched-6)', 'innogpu-fh2m-trixie postinst (patched-7)')
s=s.replace('echo "  patched-6 applies the G0M first-GPU-PLL workaround to the DKMS source object."', 'echo "  patched-7 keeps safe modesetting by default."\necho "  Experimental hardware GL path, if a Kylin root is mounted:"\necho "    sudo innogpu-install-kylin-userspace /mnt/kylin-root"')
needle='''if [ -x /usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh ]; then
    ln -sf ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh /usr/sbin/innogpu-disable-incompatible-userspace
    ln -sf ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh /usr/bin/innogpu-disable-incompatible-userspace
fi
'''
repl=needle+'''if [ -x /usr/share/innogpu-fh2m-trixie/install-kylin-userspace.sh ]; then
    ln -sf ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh /usr/sbin/innogpu-install-kylin-userspace
    ln -sf ../share/innogpu-fh2m-trixie/install-kylin-userspace.sh /usr/bin/innogpu-install-kylin-userspace
fi
if [ -x /usr/share/innogpu-fh2m-trixie/test-xorg-once.sh ]; then
    ln -sf ../share/innogpu-fh2m-trixie/test-xorg-once.sh /usr/sbin/innogpu-test-xorg-once
    ln -sf ../share/innogpu-fh2m-trixie/test-xorg-once.sh /usr/bin/innogpu-test-xorg-once
fi
'''
s=s.replace(needle, repl)
p.write_text(s)
PY

python3 - "$W/DEBIAN/prerm" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
if 'innogpu-install-kylin-userspace' not in s:
    s=s.replace('rm -f /usr/bin/innogpu-disable-incompatible-userspace /usr/sbin/innogpu-disable-incompatible-userspace\n', 'rm -f /usr/bin/innogpu-disable-incompatible-userspace /usr/sbin/innogpu-disable-incompatible-userspace\nrm -f /usr/bin/innogpu-install-kylin-userspace /usr/sbin/innogpu-install-kylin-userspace\n')
p.write_text(s)
PY

rm -rf "$W/root/DEBIAN"
cp -a "$W/DEBIAN" "$W/root/DEBIAN"
dpkg-deb --root-owner-group --build "$W/root" innogpu-fh2m-trixie_3.3.3.42-patched-7.deb
