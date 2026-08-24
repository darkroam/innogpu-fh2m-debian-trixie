# 驱动源码许可证审计

## 当前判定

**发布状态：BLOCKED（待权利与许可证材料核实）。** 本页记录仓库可直接观察到的文件头，不提供
法律意见，也不把来源包可访问等同于获得再分发授权。在阻断项关闭前，不应发布 `drivers/` 源码包，
也不应宣称整个导入源码树是开源、Dual MIT/GPL 或已获统一再分发授权。

扫描基线：2026-08-24，`drivers/` 下 484 个 Git 跟踪文件。机械扫描结果只用于发现问题，不能替代
逐文件法律审查：408 个文件同时引用来源分发中的 `GPL-COPYING` 和 `MIT-COPYING`；其余文件可能
没有可机器识别的许可证头，不能据此推断为无版权或采用项目根 MIT。

## 已确认的例外

| 文件 | 文件头声明 | 当前处理 |
| --- | --- | --- |
| `drivers/innosrvkm/include/pdp_drm.h` | `Strictly Confidential` | 再分发授权待来源方确认，发布阻断 |
| `drivers/innosrvkm/include/pvrsrv_firmware_boot.h` | `Strictly Confidential` | 再分发授权待来源方确认，发布阻断 |
| `drivers/innosrvkm/include/rgxlayer_impl.h` | `Strictly Confidential` | 再分发授权待来源方确认，发布阻断 |
| `drivers/innopmbus/innopmbus_drv.c` | BSD-3-Clause / LGPL-2.1-only 双许可 | 保留文件头，补齐适用许可证文本后复核 |
| `drivers/innopmbus/innopmbus_drv.h` | BSD-3-Clause / LGPL-2.1-only 双许可 | 保留文件头，补齐适用许可证文本后复核 |

## 阻断项

1. 向 Deepin/Innosilicon/Imagination 或可验证的来源材料确认 3 个 confidential 文件是否允许在本仓库
   公开再分发；记录授权主体、适用版本、范围和证据。无法确认时应从可发布源码边界排除这些文件，
   但任何移动或替代都必须先验证构建与功能影响。
2. 从权威来源取得并审查源码头引用的 `GPL-COPYING`、`MIT-COPYING`，以及 BSD-3-Clause、
   LGPL-2.1-only 的完整许可证文本；确认应放置的位置和随发行物携带方式。
3. 对 484 个跟踪文件生成逐文件清单，至少记录路径、版权方、原始声明、SPDX 映射、来源包路径、
   修改状态和再分发判定。无许可证头文件必须单独核实，不能继承根目录 MIT。
4. 对 `binary-manifest.json` 的 192 项第三方载荷另行完成逐项权利核实；`vendor-binary` 仅为来源分类。

## 维护规则

- 导入源码修改必须保留原始版权和许可证头；本项目贡献不能覆盖或替换上游声明。
- 根 `LICENSE` 仅适用于本项目有权授权的原创辅助工作。
- 许可证审计完成前，安装和本地验证可以继续，但 release 附件、源码归档和第三方载荷上传必须单独
  通过再分发审查。
- 每次更新来源包、`drivers/` 或 manifest 后重新运行许可证扫描并更新本页日期和数量。
