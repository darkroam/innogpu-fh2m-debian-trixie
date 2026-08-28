# 驱动源码许可证审计

> **发布状态：BLOCKED**（仓库整体与 driver-source 制品不发布；`project-tools` 仅为**候选制品**
> （机械门禁 CLEARED；当前不作为发布目标——发布决策 1C 见 [licensing.md](licensing.md) §4.1；且
> GitHub 主分支本身仍分发阻断路径，仓库级发布未闭环）。
> 本页只汇总仓库可直接观察到的声明，不提供法律意见，也不把源码或许可证文本可访问等同于获得
> 再分发授权。权威许可边界见 [licensing.md](licensing.md)。

扫描基线：2026-08-26。`drivers/` 共 484 个 Git 跟踪路径，其中 `drivers/README.md` 是项目文档
（原创层 GPL-3.0-or-later）；其余 483 个是导入的实现/构建文件。确定性扫描结果如下：

| 类别 | 路径数 | 机械映射 | 发布含义 |
| --- | ---: | --- | --- |
| 项目文档 | 1 | GPL-3.0-or-later（原创层） | 仅 `drivers/README.md` |
| Dual MIT/GPLv2 完整声明 | 408 | `MIT OR GPL-2.0-only` | 保留原声明；可进入 driver-source 制品 |
| Strictly Confidential | 3 | `LicenseRef-Strictly-Confidential`（观察标签） | 无再分发授权结论，**排除出公开制品** |
| BSD/LGPL 双声明 | 2 | `BSD-3-Clause OR LGPL-2.1-only` | 保留原声明；可进入 driver-source 制品 |
| 无机器可识别声明 | 70 | `NOASSERTION` | 不继承任何根许可证，**排除出公开制品** |

分类合计 `1 + 408 + 3 + 2 + 70 = 484`；实现/构建文件合计 483。`Dual MIT/GPLv2` 只有在声明文字
及 `GPL-COPYING`、`MIT-COPYING` 两个引用全部存在时才成立，残缺头会使审计失败。

## 机器证据

- [license-audit-policy.json](../../license-audit-policy.json)：许可层、分类允许集合、标准文本
  hash、允许 SPDX 集合、制品 allowlist 路径与期望统计。
- [source-license-inventory.tsv](source-license-inventory.tsv)：484 行逐文件记录（路径、内容
  SHA-256、分类、原始声明、规范化 SPDX、引用文本、`MODULE_LICENSE` 元数据、观察到的版权行）。
- [audit-licenses.py](../../tools/audit-licenses.py)：从 `git ls-files` 重建清单并检查语义漂移；
  [run-license-audit-tests.sh](../../tests/unit/run-license-audit-tests.sh) 覆盖正反例。
- [project-tools-allowlist.txt](project-tools-allowlist.txt) 与
  [driver-source-allowlist.txt](driver-source-allowlist.txt)：两个发布制品的精确允许清单
  （机械生成，与审计器双向校验）。
- [build-release-archive.py](../../tools/build-release-archive.py)：确定性归档构建器（从 HEAD
  读 blob/mode、要求干净树、保留 0644/0755、拒绝符号链接/重复/输出落仓；仅 CLEARED 制品可发布
  构建，`--draft` 只输出 `archive_draft=OK`）。
- [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)：第三方声明（上游 MIT 全文、drivers/
  逐文件声明、本地载荷排除；非授权声明）。

```text
license_audit_overall=PASS
license_release_gate=BLOCKED
license_artifact_project-tools_gate=CLEARED
license_artifact_driver-source_gate=BLOCKED
```

两行必须同时读取：审计 PASS 只证明“当前内容与已审策略一致”，不代表仓库可整体发布。发布制品
必须另行使用 `python3 tools/audit-licenses.py --artifact <name> --require-releasable`。

## 明确例外与阻断路径

| 文件 | 文件头声明 | 当前处理 |
| --- | --- | --- |
| `drivers/innosrvkm/include/pdp_drm.h` | `Strictly Confidential` | 排除出公开制品 |
| `drivers/innosrvkm/include/pvrsrv_firmware_boot.h` | `Strictly Confidential` | 排除出公开制品 |
| `drivers/innosrvkm/include/rgxlayer_impl.h` | `Strictly Confidential` | 排除出公开制品 |
| `drivers/innopmbus/innopmbus_drv.c` | BSD-3-Clause / LGPL-2.1-only 双许可声明 | 映射为 `BSD-3-Clause OR LGPL-2.1-only`，保留原头；`MODULE_LICENSE=Dual BSD/GPL` 元数据冲突已记录 |
| `drivers/innopmbus/innopmbus_drv.h` | BSD-3-Clause / LGPL-2.1-only 双许可声明 | 同上 |
| `drivers/innovpu/innovpu_drv.c` | Dual MIT/GPLv2；`MODULE_LICENSE=Dual BSD/GPL` 冲突 | 保留 MIT/GPL 双许可，冲突已记录 |

70 个无许可路径的完整列表见 inventory 中 `content_class=unclassified` 行（含 `drivers/Kbuild`、
各子系统 `Makefile`、`dkms.conf`、`compat_kernel6.h`、`hal_power.c` 等构建/实现文件）。

## 未关闭的阻断项

1. 3 个 `Strictly Confidential` 文件无再分发授权结论 → **从公开制品排除**；如需发布，须取得
   权利方书面授权或验证公开副本后替换，不得靠免责声明或 GPL 强行放行。
2. 70 个无许可文件的来源与适用许可未核实 → **不自动推定 MIT/GPL**，从公开制品排除；不能仅因
   位于 deb 或同目录而重许可。
3. 408 + 2 个已有声明文件的来源包随附材料与授权链逐项复核未完成 → driver-source 制品
   **BLOCKED**，不假 PASS；project-tools 不含 drivers/，不受影响。
4. 192 项 manifest 第三方载荷 → `vendor-binary` 不是许可证；**二进制 deb 与 vendor 载荷不作为
   当前发布目标**，不建立未计划发布载荷的权利登记流程。
5. 历史 patched-1.deb 属上游引入、fork 继承的上游历史（提交 9565da1/f40ad3e，2026-02-08，
   Tim Hant；darkroam fork 创建 2026-05-28），origin/main 当前不含该对象；**非阻断项**，
   不执行历史重写。

## 更新流程

1. 修改策略允许集合、分类或期望统计，说明事实来源。
2. 运行 `python3 tools/audit-licenses.py --write-inventory --write-allowlists`，复审逐路径 diff。
3. 运行 `python3 tools/audit-licenses.py`、`bash tests/unit/run-license-audit-tests.sh` 和
   `bash scripts/check-docs.sh`。
4. 只有人工阻断项有证据并经独立复审后，才可讨论把对应制品状态从 `BLOCKED` 改为 `CLEARED`；
   `status=CLEARED` 本身不是授权依据，机械门禁必须同时通过。
