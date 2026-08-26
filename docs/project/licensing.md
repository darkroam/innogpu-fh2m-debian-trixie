# 许可证与再分发边界

本文定义仓库各内容域的许可证适用范围。它记录可观察事实和工程门禁，不提供法律意见，也不以
许可证文本存在来推导来源、授权链或再分发权。

## 适用范围

仓库不是“多许可证同时适用于全部文件”。许可证按路径和文件声明分配；双许可表达式中的 `OR`
表示接收者可按其中一个许可使用对应文件，不是要求把两套许可同时套到整个项目。

| 内容范围 | 适用声明 | 是否覆盖其他范围 |
| --- | --- | --- |
| 本项目有权授权的原创文档、脚本、工具、测试、配置和辅助工作，以及 `drivers/README.md` | 根 [LICENSE](../../LICENSE) 的 MIT | 否；不覆盖导入源码或载荷 |
| `drivers/` 中逐文件清单标为 `dual-mit-gpl` 的 408 个文件 | `MIT OR GPL-2.0-only` | 仅对应清单路径；保留原文件版权与声明 |
| `drivers/innopmbus/innopmbus_drv.c`、`drivers/innopmbus/innopmbus_drv.h` | 文件声明映射为 `BSD-3-Clause OR LGPL-2.1-only` | 仅这两个文件；仍待来源与权利链复核 |
| 3 个标为 `Strictly Confidential` 的文件 | `LicenseRef-Strictly-Confidential` 仅是观察标签，**不是开源许可证或授权** | 无；再分发权未确认，直接阻断发布 |
| `drivers/` 中 70 个无机器可识别声明的实现/构建文件 | `NOASSERTION` | 不继承根 MIT，不代表无版权 |
| `binary-manifest.json` 的 192 项黑盒/用户态/固件/DDX/配置载荷 | `vendor-binary` 仅是未决来源分类 | 不是 SPDX、许可证名称或再分发授权 |

逐路径归属以 [source-license-inventory.tsv](source-license-inventory.tsv) 为准，分类规则与允许集合以
仓库根 [license-audit-policy.json](../../license-audit-policy.json) 为准。摘要文档不能覆盖这两个机器
文件中的路径边界。

## GPL 边界

GPL 不会自动把无权授权的内容变成 GPL，也不会补足 confidential、`NOASSERTION` 或二进制载荷的
再分发权。408 个双许可文件可选择 GPL-2.0-only 路径，但这个选择只作用于对应文件。把这些源码与
Linux 内核、其他导入源码和预编译对象组合成模块时，衍生作品边界、许可证兼容性和源码提供义务仍须
单独审查；本项目不得把不拥有权利的内容单方面重许可为 GPL。源码中的 10 处
`MODULE_LICENSE(...)` 只是内核模块元数据，不替代逐文件许可；其中两处与文件头不一致，已在
逐文件 inventory 中作为阻断冲突记录。

## 许可证文本

[`LICENSES/`](../../LICENSES/README.md) 保存上述导入文件声明涉及的标准条款副本。其作用是让条款
随仓库可读，不改变适用路径，不替代源文件中的版权声明，也不证明 Deepin/Innosilicon/Imagination/
Chips&Media 的授权链。特别是，文本文件存在不解除 confidential、未分类源码或 192 项载荷的阻断。

导入源码头使用 `GPL-COPYING`/`MIT-COPYING` 名称引用来源分发材料；本仓库提供标准 GPL-2.0-only
与 MIT 条款，但仍须把来源包实际随附文本、来源和适用版本纳入人工权利审查。

## 自动审计与发布门禁

```sh
python3 tools/audit-licenses.py
python3 tools/audit-licenses.py --require-releasable
```

第一条校验逐文件 hash、声明分类、修改 provenance、许可证文本 hash、manifest 许可证字段和已提交
inventory 是否一致。当前预期为 `license_audit_overall=PASS` 与
`license_release_gate=BLOCKED`：前者只说明机械记录一致，后者才是发布决策。

第二条是发布门禁；在权利证据未完成前必须以 rc=2 和
`license_release_readiness=FAIL reason=release_gate_blocked` 结束。不得为了让 release 流程变绿而把
策略状态改为 `CLEARED`；状态变化必须同时关闭全部阻断项并接受单独法律/监督复审。

## 维护要求

- 导入或修改 `drivers/`、`binary-manifest.json`、许可证材料后，先更新策略中的明确允许集合和
  provenance，再运行 `python3 tools/audit-licenses.py --write-inventory`，复审 diff 后执行检查模式。
- manifest 的 `license` 必须是非空字符串。`vendor-binary` 保持未决；任何 SPDX 替换都必须在策略中
  逐路径登记可验证的证据，不能只改 JSON 字段。
- 导入源码修改必须保留原始版权和许可证声明；本项目贡献不能覆盖或替换上游声明。
- release 附件、源码归档和第三方载荷上传必须通过 `--require-releasable`，本地构建/验证不等于
  获得公开再分发权。

当前阻断项及逐文件统计见 [源码许可证审计](source-license-audit.md)。
