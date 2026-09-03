# 2026-09-03 启动报错归因

## 现象

在重启后的内核 journal 中反复出现 AMD-Vi、microcode、ACPI、SRSO、Innogpu
`hwinfo` 和 DMA debugfs 告警。用户 OCR 将 DMA debugfs 的父目录识别为
`drmengine`；原始 journal 的实际文本是 `dmaengine`。

## 证据基线

本次只读复盘核对了以下 6 个启动证据，覆盖 2026-09-02 11:50 至
2026-09-03 16:13：

| 证据 | 上下文 | 代表性行号 |
| --- | --- | --- |
| `build/r03-b1-kernel.log` | patch-024 首次安装后启动 | AMD-Vi 203；SRSO 223；hwinfo 966；DMA 1031 |
| `build/r05-evidence/i2-post-s2idle-kernel.log` | patch-026 运行证据 | AMD-Vi 203；SRSO 223；hwinfo 980；DMA 1057 |
| `build/r06-evidence/round1-4.0.1-i3-20260902-170142/pre-kernel-journal.txt` | 4.0.1-i3 deep 前 | AMD-Vi 203；SRSO 223；hwinfo 1012；DMA 1073 |
| `build/r06-evidence/round1-4.0.1-i3-20260902-170142/post-kernel-journal.txt` | 4.0.1-i3 deep 后 | AMD-Vi 203；SRSO 223；hwinfo 1012；DMA 1073 |
| `build/r12-stage2/r12-display-failure-20260903-120631/post-kernel-journal.txt` | 4.0.2-i2 启动/恢复证据 | AMD-Vi 203；SRSO 223；hwinfo 962；DMA 1028 |
| `build/r13-stage2-20260903-post-reboot/kernel-journal-host.txt` | 4.0.2-i3 启动证据 | AMD-Vi 203；SRSO 223；hwinfo 998；DMA 1063 |

每一类下表中的消息均在 6/6 个启动证据中出现；行号因同一 journal 摘录内容
和少量上下文不同而变化。6 次启动逐字一致，且覆盖 patch-024、patch-026、
patch-028、patch-029 前后的版本边界。

## 归因表

| # | 报错类别 | 归因 | 证据与处理建议 |
| --- | --- | --- | --- |
| 1 | `AMD-Vi: [Firmware Bug]: No southbridge IOAPIC found` / `Disabling interrupt remapping` | pre-existing；系统/BIOS 平台级 | 6/6 boot 均出现，R13 行 203。未触及本仓库驱动路径；文档化，后续由 BIOS/平台侧处理。 |
| 2 | `microcode: no support for this CPU vendor` | pre-existing；系统级 | 6/6 boot 均出现，R13 行 632。不是本仓库补丁可修复的 GPU 驱动问题；文档化。 |
| 3 | `Speculative Return Stack Overflow: IBPB-extending microcode not applied!` 及其 Vulnerable/Safe RET 消息 | pre-existing；系统/CPU microcode 级 | 6/6 boot 均出现，R13 行 223-226。用户 OCR 未列出；按内核安全缓解与固件边界记录，不能由本仓库静默消除。 |
| 4 | `ACPI: video: [Firmware Bug]: ACPI(PEG0) defines _DOD but not _DOS` | pre-existing；系统/BIOS ACPI 级 | 6/6 boot 均出现，R13 行 670。用户 OCR 未列出；记录为 ACPI 固件问题，不修改驱动绕过。 |
| 5 | `firmware: failed to load innogpu/hwinfo_g0m.bin (-2)`（3 次） | pre-existing；厂商固件载荷缺失 | 6/6 boot 均出现，R13 行 998-1000；现有厂商包未提供该文件。保留为厂商固件渠道问题；若后续获得可验证候选，另行批准安装验证。 |
| 6 | `hwinfo register no pass` / `dev_rsrc is NULL or hwinfo register fail` 级联消息 | pre-existing；#5 的厂商固件缺失后果 | 6/6 boot 均出现，R13 行 1001、1003-1006。属于同一失败链，不新增补丁或把级联日志误判为本仓库回归。 |
| 7 | `can't get dp/hdmi/vga hwinfo version, skip hwinfo!` | pre-existing；厂商 hwinfo 降级路径 | 6/6 boot 均出现，R13 行 1041-1045。驱动跳过缺失 hwinfo，继续其既有默认路径；文档化并纳入厂商报告。 |
| 8 | `debugfs: Directory '0000:02:00.0' with parent 'dmaengine' already present!` | pre-existing；厂商 DMA 双引擎注册冲突 | 6/6 boot 均出现，R13 行 1063。OCR 校正为 `dmaengine`；AXI DMA 与 PCIe DMA 均调用 `dma_async_device_register()`，且注册到同一 PCI 设备名空间，见 `drivers/innodma/inno_axi_dma_drv.c:732`、`drivers/innodma/inno_pcie_dma_drv.c:635`。patch-029 不涉及 dmaengine；文档化并纳入厂商报告。 |
| 9 | HDMI `hwinfo version failed` / `get_output_mode failed!!!, Using the default output:0`（2 个实例） | pre-existing；厂商默认输出降级路径 | 6/6 boot 均出现，R13 行 1079-1085。patch-029 不修改 output-mode 查询；保持既有默认输出行为，后续按厂商 hwinfo 缺失问题报告。 |
| 10 | DP `hwinfo version failed, Use default DDCCI`（2 个实例） | pre-existing；厂商默认 DDCCI 降级路径 | 6/6 boot 均出现，R13 行 1094-1097。patch-029 只处理该模式下 panel 恢复可达性，不伪造亮度设备或把 DDCCI 改成 PWM；显示/恢复结论以 R13 阶段 2 记录为准。 |

## 根因边界

上述 10 类全部判定为 pre-existing，`ours=0`。对比 patch-024、patch-026、
patch-028、patch-029 的 diff 路径，没有任何补丁触及 AMD-Vi、microcode、ACPI、
SRSO、hwinfo 固件加载/注册、厂商 connector 默认降级或 dmaengine 注册路径。
其中 patch-029 仅修改 DDCCI panel 创建条件和 DDCCI backlight 初始化的显式
early-return；它不会消除启动时的 hwinfo 缺失消息，也不会注册 backlight device。

## 后续门槛

- 系统/BIOS 类（AMD-Vi、microcode、SRSO、ACPI `_DOD/_DOS`）不在本仓库修复范围，保留日志并向平台/固件侧报告。
- `hwinfo_g0m.bin` 缺失和由此产生的默认路径，等待可验证厂商固件来源；安装候选、重启和功能验证必须另行批准。
- `dmaengine` 目录冲突需要厂商 DMA 驱动注册设计的独立分析；本记录不把它作为 DRM 或 patch-029 问题。
- 本记录只归因启动告警，不把“告警消失”当作显示或 suspend/resume 修复验收；当前运行结论仍以 `docs/project/status.md` 为准。

## 结论

R15 不新增补丁。现有证据足以将用户报告及 journal 中补充发现的 10 类启动
告警全部归入 pre-existing；后续工作是厂商/平台报告和可验证固件来源调查。
