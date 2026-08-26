# 驱动源码许可证审计

## 当前判定

**发布状态：BLOCKED（待权利与来源材料核实）。** 本页只汇总仓库可直接观察到的声明，不提供法律
意见，也不把源码或许可证文本可访问等同于获得再分发授权。在阻断项关闭前，不应发布完整
`drivers/` 源码包或第三方载荷，也不应宣称整个导入树采用统一开源许可证。

扫描基线：2026-08-26。`drivers/` 共 484 个 Git 跟踪路径，其中 `drivers/README.md` 是根 MIT 范围
内的项目文档；其余 483 个是导入的实现/构建文件。确定性扫描结果如下：

| 类别 | 路径数 | 机械映射 | 发布含义 |
| --- | ---: | --- | --- |
| 项目文档 | 1 | MIT（根 LICENSE） | 仅 `drivers/README.md` |
| Dual MIT/GPLv2 完整声明 | 408 | `MIT OR GPL-2.0-only` | 保留原声明；来源与授权链仍需复核 |
| Strictly Confidential | 3 | `LicenseRef-Strictly-Confidential`（观察标签） | 无再分发授权结论，发布阻断 |
| BSD/LGPL 双声明 | 2 | `BSD-3-Clause OR LGPL-2.1-only` | 保留原声明；来源与授权链仍需复核 |
| 无机器可识别声明 | 70 | `NOASSERTION` | 不继承根 MIT，发布阻断 |

分类合计为 `1 + 408 + 3 + 2 + 70 = 484`；实现/构建文件合计为 `408 + 3 + 2 + 70 = 483`。
`Dual MIT/GPLv2` 只有在声明文字及 `GPL-COPYING`、`MIT-COPYING` 两个引用全部存在时才成立，残缺
头会使审计失败。

## 机器证据

- [license-audit-policy.json](../../license-audit-policy.json)：分类允许集合、标准文本 hash、已修改路径到
  patch 编号的映射、manifest 未决值、固定汇总和发布阻断项。
- [source-license-inventory.tsv](source-license-inventory.tsv)：484 行逐文件记录，包含路径、内容 SHA-256、
  原始声明、规范化 SPDX 表达式、引用文本、来源、修改状态、patch 编号、再分发审查状态和观察到的
  版权行。
- [audit-licenses.py](../../tools/audit-licenses.py)：从 `git ls-files` 重建清单并检查语义漂移；
  [run-license-audit-tests.sh](../../tests/unit/run-license-audit-tests.sh) 覆盖 11 项正反例。

```text
license_audit_overall=PASS
license_release_gate=BLOCKED
```

两行必须同时读取。审计 PASS 只证明“当前内容与已审策略一致”，不代表可发布。发布流程必须另行使用
`python3 tools/audit-licenses.py --require-releasable`；当前应失败。

## 明确例外

| 文件 | 文件头声明 | 当前处理 |
| --- | --- | --- |
| `drivers/innosrvkm/include/pdp_drm.h` | `Strictly Confidential` | 再分发授权待确认，发布阻断 |
| `drivers/innosrvkm/include/pvrsrv_firmware_boot.h` | `Strictly Confidential` | 再分发授权待确认，发布阻断 |
| `drivers/innosrvkm/include/rgxlayer_impl.h` | `Strictly Confidential` | 再分发授权待确认，发布阻断 |
| `drivers/innopmbus/innopmbus_drv.c` | BSD-3-Clause / LGPL-2.1-only 双许可声明 | 映射为 `BSD-3-Clause OR LGPL-2.1-only`，保留原头并复核权利链 |
| `drivers/innopmbus/innopmbus_drv.h` | BSD-3-Clause / LGPL-2.1-only 双许可声明 | 同上 |

## 已完成的工程工作

- 已生成 484 路径逐文件 inventory，并固定内容 hash；70 个未分类文件不再隐藏在“其余文件”中。
- 已把 28 个项目修改路径映射到 patch-001/002/006/007/009/023/025/026/027 provenance；映射来源为
  [patch-provenance.md](../planning/patch-provenance.md)。修改状态不改变原文件许可证。
- 已在 [`LICENSES/`](../../LICENSES/README.md) 放置 MIT、GPL-2.0-only、BSD-3-Clause、
  LGPL-2.1-only 标准条款副本并固定 hash。条款可用性已改善，但不据此关闭权利阻断。
- 已校验 `binary-manifest.json` 192 项均有 `license` 字段，且全部仍为未决标记 `vendor-binary`；
  审计器会拒绝无策略证据的“已解析”许可证值。
- 已单列 10 处 `MODULE_LICENSE(...)` 内核模块元数据；其中 `innopmbus_drv.c` 的文件头
  BSD/LGPL 与元数据 BSD/GPL、`innovpu_drv.c` 的文件头 MIT/GPL 与元数据 BSD/GPL 不一致，均标为
  `blocked-declaration-conflict`。模块元数据不替代文件许可证声明。

## 未关闭的阻断项

1. 向 Deepin/Innosilicon/Imagination 或其他可验证权利方确认 3 个 confidential 文件能否公开再分发；
   记录授权主体、适用版本、范围和证据。无法确认时，应设计并验证排除这些文件的可发布边界。
2. 逐项核实 70 个 `NOASSERTION` 实现/构建文件的来源、版权和适用许可，不能从相邻文件或根 MIT
   推断。
3. 对 408 + 2 个已有声明文件复核来源包随附材料、授权链及本项目修改后的合规义务；标准文本副本
   不替代该审查；同时澄清并修复上述两处文件头/模块元数据冲突，未经权利方证据不得任选解释。
4. 对 manifest 的 192 项第三方载荷逐项取得许可证和再分发证据；`vendor-binary` 不是许可证。

## 更新流程

1. 修改策略允许集合、provenance 或逐项证据，说明事实来源。
2. 运行 `python3 tools/audit-licenses.py --write-inventory`，复审 inventory 的逐路径 diff。
3. 运行 `python3 tools/audit-licenses.py`、`bash tests/unit/run-license-audit-tests.sh` 和
   `bash scripts/check-docs.sh`。
4. 只有全部人工阻断项有证据并经独立复审后，才可讨论把策略状态从 `BLOCKED` 改为 `CLEARED`。
