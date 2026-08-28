# Third-Party License Texts

This directory carries standard license texts referenced by the repository.
Their presence does not apply all licenses to the whole repository: each layer
and each imported source file keeps the license that actually covers it.
Copyright notices and the license declarations in each source file remain
authoritative.

| File | SPDX identifier | Covers |
| --- | --- | --- |
| `GPL-3.0-or-later.txt` | GPL-3.0-or-later | 本项目原创层（根 [LICENSE](../LICENSE) 顶部的 GPLv3 文本） |
| `MIT.txt` | MIT | fork 上游继承层（`Copyright (c) 2026 Tim Hant`，随 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) 保留） |
| `GPL-2.0-only.txt` | GPL-2.0-only | `drivers/` 中 `MIT OR GPL-2.0-only` 双许可文件引用的 GPLv2 条款 |
| `BSD-3-Clause.txt` | BSD-3-Clause | `drivers/` 中 `BSD-3-Clause OR LGPL-2.1-only` 双许可文件引用的条款 |
| `LGPL-2.1-only.txt` | LGPL-2.1-only | 同上（PMBus 源码对） |
| `MPL-2.0.txt` | MPL-2.0 | `components/picom/001-probe-explicit-uniform-location.patch`（目标文件 `gl_common.c` 的文件级声明）引用的 MPL-2.0 条款 |

规则：

- 根许可证只覆盖本项目有权许可的原创层；它不覆盖 `drivers/`、上游 MIT 内容或本地载荷。
- `drivers/` 中的 `Dual MIT/GPLv2` 声明规范化映射为 `MIT OR GPL-2.0-only`（不是 GPLv3）；
  `BSD-3-Clause OR LGPL-2.1-only` 保持原样。`OR` 是对应文件的双许可选择，不是仓库级组合。
- 标为 `Strictly Confidential` 或无许可声明的文件不因本目录获得任何许可证。
- 文本副本不建立来源/授权链；逐路径边界以
  [`docs/project/licensing.md`](../docs/project/licensing.md)（唯一权威文档）与
  [`source-license-inventory.tsv`](../docs/project/source-license-inventory.tsv) 为准。
