# Third-Party License Texts

This directory carries standard license texts referenced by imported source
files. Their presence does not apply all four licenses to the repository.
Copyright notices and the actual license declarations in each source file
remain authoritative.

| File | SPDX identifier | Purpose |
| --- | --- | --- |
| `MIT.txt` | MIT | Standard MIT terms referenced by the imported Dual MIT/GPLv2 headers |
| `GPL-2.0-only.txt` | GPL-2.0-only | GNU GPL version 2 terms referenced by the imported Dual MIT/GPLv2 headers |
| `BSD-3-Clause.txt` | BSD-3-Clause | Standard BSD 3-Clause terms declared by the PMBus source pair |
| `LGPL-2.1-only.txt` | LGPL-2.1-only | GNU LGPL version 2.1 terms declared by the PMBus source pair |

These copies make the referenced terms available with the repository. They do
not establish provenance, resolve the files marked `Strictly Confidential`,
or grant rights for the binary payloads in `binary-manifest.json`. Release
readiness remains governed by
[`docs/project/source-license-audit.md`](../docs/project/source-license-audit.md).

The exact path mapping is maintained in
[`source-license-inventory.tsv`](../docs/project/source-license-inventory.tsv):
`dual-mit-gpl` means `MIT OR GPL-2.0-only`, while `bsd-lgpl-dual` means
`BSD-3-Clause OR LGPL-2.1-only`. `OR` is a choice for those files, not a
repository-wide combination. Unclassified, confidential, and vendor payload
entries do not inherit a license from this directory.
