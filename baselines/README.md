# Baselines

This directory is intentionally kept small and should be committed.

Keep:

- `final-summary.md`
- `latest-*-result.txt`
- `latest-*/result.txt`
- `latest-phase4-*.txt` 和经脱敏的 Phase 4 正式审计日志
- `latest-runtime-baseline.txt`（当前 runtime 权威摘要）
- `runtime-results-YYYYMMDD.txt`（人工真机结果输入；只保留脱敏、机器可读条目）

Do not track in Git:

- Raw Xorg logs.
- Full `glxinfo` / `xdpyinfo` dumps.
- Host-specific debug captures with local paths.

The committed files record the final pass/fail state needed to understand the repository baseline without carrying large, machine-specific logs. `latest-runtime-baseline.txt` is the authority for the current 35-item capability summary; its `tested_commit` identifies the code under test, not the later commit that archives the evidence.

Runtime scripts may leave ignored raw files in this directory for local diagnosis. Their presence in the working tree does not make them repository evidence; only `git ls-files baselines/` defines the committed baseline set.

Important: committed `latest-*/result.txt` files are evidence with a version identity only when their filename
or content records it. The `latest-phase4-*.txt` markers are versioned (4.0.0-i1, Phase 4 acceptance and
rollback drill) and may be cited for that version. The older `latest-*` results (e.g. `latest-desktop-hwgl-
result.txt`) originated from the patched-17 validation period and contain no version identity; they must never
be cited as proof for a newer package. Scripts that enable hardware GL must require a local runtime test
artifact, such as `baselines/latest-ddx-test/summary.txt`, before changing the current machine.
