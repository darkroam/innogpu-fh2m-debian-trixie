# Baselines

This directory is intentionally kept small and should be committed.

Keep:

- `final-summary.md`
- `latest-*-result.txt`
- `latest-*/result.txt`

Do not keep:

- Raw Xorg logs.
- Full `glxinfo` / `xdpyinfo` dumps.
- Host-specific debug captures with local paths.

The committed files record the final pass/fail state needed to understand the repository baseline without carrying large, machine-specific logs.

Important: committed `latest-*/result.txt` files are historical evidence only. Scripts that enable hardware GL must require a local runtime test artifact, such as `baselines/latest-ddx-test/summary.txt`, before changing the current machine.
