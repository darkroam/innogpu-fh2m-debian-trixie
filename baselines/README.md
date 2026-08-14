# Baselines

This directory is intentionally kept small and should be committed.

Keep:

- `final-summary.md`
- `latest-*-result.txt`
- `latest-*/result.txt`

Do not track in Git:

- Raw Xorg logs.
- Full `glxinfo` / `xdpyinfo` dumps.
- Host-specific debug captures with local paths.

The committed files record the final pass/fail state needed to understand the repository baseline without carrying large, machine-specific logs.

Runtime scripts may leave ignored raw files in this directory for local diagnosis. Their presence in the working tree does not make them repository evidence; only `git ls-files baselines/` defines the committed baseline set.

Important: committed `latest-*/result.txt` files are historical evidence only. Scripts that enable hardware GL must require a local runtime test artifact, such as `baselines/latest-ddx-test/summary.txt`, before changing the current machine.
