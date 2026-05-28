#!/bin/bash
set -euo pipefail
cd /home/ok/src/innogpu-fh2m-debian-trixie
W=$(mktemp -d /tmp/innogpu-pkg6.XXXXXX)
mkdir -p "$W/root" "$W/DEBIAN"
dpkg-deb -x innogpu-fh2m-trixie_3.3.3.42-patched-5.deb "$W/root"
dpkg-deb -e innogpu-fh2m-trixie_3.3.3.42-patched-5.deb "$W/DEBIAN"
python3 - "$W/root/usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); b=bytearray(p.read_bytes())
old=bytes.fromhex('e8 09 fd ff ff'); new=bytes.fromhex('90 90 90 90 90')
hits=[]; start=0
while True:
    i=b.find(old,start)
    if i<0: break
    hits.append(i); start=i+1
if len(hits)==1:
    off=hits[0]; b[off:off+5]=new; p.write_bytes(b); print(f'patched object at {off:#x}')
elif b.find(new)>=0:
    print('object already patched')
else:
    raise SystemExit(f'unexpected old pattern hits: {hits}')
PY
install -m 0755 scripts/patch-skip-first-gpupll.sh "$W/root/usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh"
install -m 0755 scripts/disable-incompatible-userspace.sh "$W/root/usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh"
ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/bin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh "$W/root/usr/sbin/innogpu-skip-first-gpupll"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/bin/innogpu-disable-incompatible-userspace"
ln -sfn ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh "$W/root/usr/sbin/innogpu-disable-incompatible-userspace"
python3 - "$W/DEBIAN/control" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
s=s.replace('Version: 3.3.3.42-patched-5','Version: 3.3.3.42-patched-6')
s=s.replace('compatibility fixes plus Debian 6.12.88/6.12.90 runtime workarounds.', 'compatibility fixes, built-in G0M PLL workaround, and Mesa-safe userspace cleanup.')
p.write_text(s)
PY
cat > "$W/DEBIAN/postinst" <<'POST'
#!/bin/bash
set -e
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
KERNEL_VER="$(uname -r)"
echo ""
echo "========================================="
echo "  innogpu-fh2m-trixie postinst (patched-6)"
echo "========================================="
DKMS_BIN="$(command -v dkms || true)"
if [ -z "$DKMS_BIN" ] && [ -x /usr/sbin/dkms ]; then DKMS_BIN=/usr/sbin/dkms; fi

echo "[1/9] Ensuring helper commands are linked..."
if [ -x /usr/share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh ]; then
    ln -sf ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh /usr/sbin/innogpu-skip-first-gpupll
    ln -sf ../share/innogpu-fh2m-trixie/patch-skip-first-gpupll.sh /usr/bin/innogpu-skip-first-gpupll
fi
if [ -x /usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh ]; then
    ln -sf ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh /usr/sbin/innogpu-disable-incompatible-userspace
    ln -sf ../share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh /usr/bin/innogpu-disable-incompatible-userspace
fi

echo "[2/9] Registering DKMS module..."
if [ -n "$DKMS_BIN" ]; then
    "$DKMS_BIN" add innogpu-kernel/2.2 2>/dev/null || true
    echo "[3/9] Building for kernel $KERNEL_VER (may take a few minutes)..."
    if "$DKMS_BIN" build innogpu-kernel/2.2 -k "$KERNEL_VER" --force 2>&1; then
        echo "      Build successful!"
        echo "[4/9] Installing DKMS module..."
        "$DKMS_BIN" install innogpu-kernel/2.2 -k "$KERNEL_VER" --force 2>&1
        echo "      Module installed!"
    else
        echo "WARNING: DKMS build failed."
        echo "         You may need: sudo apt install linux-headers-$KERNEL_VER"
    fi
else
    echo "WARNING: dkms not found. Install: sudo apt install dkms"
fi

echo "[5/9] Configuring innogpu module options..."
mkdir -p /etc/modprobe.d
printf '%s\n' 'options innogpu firmware_en=1' > /etc/modprobe.d/innogpu.conf

echo "[6/9] Leaving boot autoload disabled until manual modprobe succeeds..."
if [ -f /etc/modules-load.d/innogpu.conf ]; then
    mv -f /etc/modules-load.d/innogpu.conf /etc/modules-load.d/innogpu.conf.disabled-by-package
fi

echo "[7/9] Disabling incompatible Innosilicon userspace GL/GBM..."
if [ -x /usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh ]; then
    /usr/share/innogpu-fh2m-trixie/disable-incompatible-userspace.sh || true
fi

echo "[8/9] Disabling conflicting remote-display services if enabled..."
for svc in xvfb x11vnc novnc; do
    if systemctl is-enabled "$svc" 2>/dev/null | grep -q enabled; then
        systemctl disable "$svc" 2>/dev/null || true
    fi
done

echo "[9/9] Refreshing module metadata/initramfs..."
depmod -a "$KERNEL_VER" 2>/dev/null || true
if command -v update-initramfs >/dev/null 2>&1; then update-initramfs -u -k "$KERNEL_VER" 2>/dev/null || true; fi

echo ""
echo "========================================="
echo "  Installation complete."
echo "  patched-6 applies the G0M first-GPU-PLL workaround to the DKMS source object."
echo "  Test manually before enabling reboot autoload:"
echo "    sudo modprobe innogpu"
echo ""
echo "  After manual load succeeds, enable reboot autoload:"
echo "    printf '%s\\n' innogpu | sudo tee /etc/modules-load.d/innogpu.conf"
echo "    sudo depmod -a $KERNEL_VER"
echo "    sudo update-initramfs -u -k $KERNEL_VER"
echo "    sudo reboot"
echo "========================================="
exit 0
POST
chmod 0755 "$W/DEBIAN/postinst"
python3 - "$W/DEBIAN/prerm" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
s=s.replace('dkms remove innogpu-kernel/2.2 --all 2>/dev/null || true\n', 'dkms remove innogpu-kernel/2.2 --all 2>/dev/null || true\nrm -f /usr/bin/innogpu-skip-first-gpupll /usr/sbin/innogpu-skip-first-gpupll\nrm -f /usr/bin/innogpu-disable-incompatible-userspace /usr/sbin/innogpu-disable-incompatible-userspace\n')
s=s.replace('/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.bak', '/usr/lib/x86_64-linux-gnu/dri/innogpu_dri.so.disabled')
p.write_text(s)
PY
rm -rf "$W/root/DEBIAN"
cp -a "$W/DEBIAN" "$W/root/DEBIAN"
dpkg-deb --root-owner-group --build "$W/root" innogpu-fh2m-trixie_3.3.3.42-patched-6.deb
