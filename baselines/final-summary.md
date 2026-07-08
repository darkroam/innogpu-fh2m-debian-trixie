# Final Innogpu FH2M Baseline

Date: 2026-07-08

Final package:

```text
innogpu-fh2m-trixie 3.3.3.42-patched-17
```

Confirmed results:

```text
PASS_POST_REBOOT_HWGL
PASS_DESKTOP_HWGL
PASS_CURRENT_XORG_HWGL_RUNTIME
PASS_VENDOR_DDX_RUNTIME_ACCELERATION
```

Working state:

- Driver and firmware report OK.
- `/dev/dri/card0`, `/dev/dri/renderD128`, and `/dev/fb0` are present.
- tty1 login, Xorg, startx, dwm, and dwmblocks work after reboot.
- `/etc/X11/xorg.conf` uses the `innogpu` Xorg driver.
- Deepin 202504 userspace GL is enabled.
- OpenGL renderer is `Fantasy II-M`.
- DRI3, GLX, Present, and xrandr provider reporting are available.

Rollback points:

- `innogpu-fh2m-trixie_3.3.3.42-patched-8.deb`: stable pre-Deepin rollback package.
- `innogpu-fh2m-trixie_3.3.3.42-patched-17.deb`: current successful package.

The large raw Xorg/DDX logs were removed from the repo. They were useful during debugging but contained host-specific paths and duplicated the pass/fail evidence above.
