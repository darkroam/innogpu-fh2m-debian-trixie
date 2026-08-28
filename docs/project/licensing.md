# 许可证与再分发边界（唯一权威文档）

本文是仓库许可证边界的**唯一权威文档**。其他 README/status/todo 文档只链接本文，不复制规则。
本文记录许可模型和工程门禁，不提供法律意见，也不以许可证文本存在来推导来源、授权链或再分发权。

> 权威状态字段：`license_release_gate=BLOCKED`（仓库整体不发布；`project-tools` 为**候选制品**
> （机械门禁 CLEARED，发布待监督批准），见 §4）。审计器机械校验该字段与
> `license-audit-policy.json` 的 `release_status` 一致。

## 1. 三层许可模型

仓库不是“所有许可证同时适用于全部文件”。许可证按层与路径分配：

| 层 | 范围 | 适用许可证 |
| --- | --- | --- |
| **原创层** | 本项目（darkroam fork）后续原创的框架、脚本、工具、测试、文档、配置和辅助代码，以及 `drivers/README.md`；不含从上游保留或派生的实质性内容，也不含 `drivers/` 厂商代码 | 根 [LICENSE](../../LICENSE) 的 **GPL-3.0-or-later** |
| **上游继承层** | fork 自 [timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)（起点提交 `8be37ed`）；Tim Hant 原始代码及其仍存在的实质性派生内容 | **MIT**（`Copyright (c) 2026 Tim Hant`，全文见 [LICENSES/MIT.txt](../../LICENSES/MIT.txt) 与 [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)） |
| **drivers/ 层** | `drivers/` 导入的厂商驱动源码（来源：Deepin 原包 `innogpu-fh2m_20250421190503-debug_amd64.deb` 内 `usr/src/innogpu-kernel-2.2/`） | **逐文件原声明**，见 §2 |
| **本地载荷** | `debs/`、`vendor/`、`build/`、`third_party/`、`*.deb` | **不随公开制品发布**，不适用任何仓库许可证 |

**换证不撤销既有授权**：此前已按 MIT 许可证发布的版本及副本继续保有原 MIT 权利；本仓库原创层
改用 GPL-3.0-or-later 不撤销既有授权，也不虚构其他权利主体（需要版权主体时使用仓库现有可验证
身份 `darkroam` 或 “project contributors”）。

## 2. drivers/ 逐文件许可

`drivers/` **不适用**根 GPL-3.0-or-later。每个文件保留并沿用其自身文件头声明；本项目对这些
文件的修改采用与原文件兼容的原许可，优先保持原双许可表达式：

| 文件声明 | 规范化表达式 | 处置 |
| --- | --- | --- |
| `Dual MIT/GPLv2`（含 `GPL-COPYING`/`MIT-COPYING` 双引用） | `MIT OR GPL-2.0-only`（**不是** GPLv3） | 可进入 driver-source 制品 |
| `BSD-3-Clause OR LGPL-2.1-only` | 保持原样 | 可进入 driver-source 制品（仅 `drivers/innopmbus/innopmbus_drv.{c,h}`） |
| `Strictly Confidential`（精确 3 个路径，见 §3） | 观察标签 `LicenseRef-Strictly-Confidential`，**不是许可证** | 排除出公开制品 |
| 无许可声明（精确 70 个路径，含 Kbuild/Makefile 等构建文件） | `NOASSERTION`，**不是许可证** | 不自动推定 MIT/GPL；排除出公开制品 |
| 其他标准 SPDX 行 | 精确允许集合内的表达式 | 按文件声明保留 |

- `MODULE_LICENSE(...)` 只是内核模块元数据，不替代文件许可；`innopmbus_drv.c` 与 `innovpu_drv.c`
  的文件头与元数据不一致，已在逐文件 inventory 中记录为冲突。
- Linux 内核是 GPL-2.0-only：不得把内核模块整体改成 GPL-3.0-only。
- “未被代码引用”不等于“可以公开分发”：confidential/无许可文件只要仍被 Git 跟踪，就属于仓库
  公开内容，不得进入发布制品。

精确路径边界以 [source-license-inventory.tsv](source-license-inventory.tsv) 与
[license-audit-policy.json](../../license-audit-policy.json) 为准；分类规则与允许集合以
`license-audit-policy.json` 为机器权威。

## 3. 当前公开范围与阻断路径

`drivers/` 共 **484** 个跟踪路径：1 个项目文档 + 408 双许可 + 3 confidential + 2 BSD/LGPL +
70 无许可。**不得声称从 Git 历史删除阻断内容**；处置方式是**从未来公开制品排除**：

- **Strictly Confidential ×3**（排除出公开制品）：
  - `drivers/innosrvkm/include/pdp_drm.h`
  - `drivers/innosrvkm/include/pvrsrv_firmware_boot.h`
  - `drivers/innosrvkm/include/rgxlayer_impl.h`
- **无许可 ×70**（排除出公开制品）：见 [source-license-audit.md](source-license-audit.md) 与
  inventory 中 `unclassified` 行。
- **本地载荷**：`debs/`、`vendor/`、`build/`、`third_party/` 与 `*.deb` 由 `.gitignore` 排除，
  不随公开制品发布；`binary-manifest.json` 的 `vendor-binary` 是来源分类，**不是许可证**。
- **未发布二进制权利方推测**：不为未计划发布的 192 项载荷建立权利登记流程；二进制 deb 与
  vendor 载荷不作为当前发布目标。

## 4. 发布制品

仓库内容与发布制品分开。只维护两个制品（CLI 也只声明这两个）：

| 制品 | 内容 | 状态 | 说明 |
| --- | --- | --- | --- |
| `project-tools` | **候选制品**：**失败关闭分类**生成的允许清单（见 [project-tools-allowlist.txt](project-tools-allowlist.txt)）= 已批准原创前缀（`.github/`/`baselines/`/`docs/`/`scripts/`/`tests/`/`tools/`，GPL-3.0-or-later）+ 显式路径映射（上游继承层 MIT：`.gitignore`/`README.md`/`scripts/install.sh`；`LICENSES/` 标准文本组：MIT/GPL-2.0-only/BSD-3-Clause/LGPL-2.1-only/MPL-2.0；`components/` 第三方派生组）；**任何不在前缀/映射内的路径一律拒绝**（无全局默认 GPL） | **CLEARED**（仅机械门禁；发布待监督批准） | `components/` 许可材料已封存：picom 补丁按目标文件级 **MPL-2.0**（Copyright (c) Yuxuan Shui），`picom.conf` 为原创层 GPL-3.0-or-later；fbterm 补丁为 GPL-2.0-only（(C) 2008 dragchan）；NOTICE 门禁按**路径组**绑定版权/许可标记；`patches/`（混合/未决许可，整体排除）、`debs/`（整目录）、`drivers/`、`vendor/`、`build/`、`third_party/` 不含；GitHub 主分支仍分发阻断路径，仓库级发布未闭环（§4.1） |
| `driver-source` | `drivers/` 中仅具有明确许可声明的路径（408 dual + 2 BSD/LGPL + `drivers/README.md`）；allowlist 见 [driver-source-allowlist.txt](driver-source-allowlist.txt) | **BLOCKED** | 排除 confidential ×3 与无许可 ×70 后**无法独立构建**（缺 Kbuild 等构建文件），**不是完整驱动**；缺失内容须由用户按原声明从本地原包取得，不假 PASS；再分发权利链待监督复审 |

`local-extractor` 不再是独立制品/门禁：本地载荷提取与校验工具是 `project-tools` 的一个功能，
见 [docs/user/local-extractor.md](../user/local-extractor.md)。

### 4.1 GitHub 主分支发布面（P1 阻断）

GitHub 仓库 `main` 分支本身是公开分发面：clone / GitHub 源码归档直接分发全部 706 个跟踪路径，
**不经 `build-release-archive.py`**，因此当前仍公开分发 3 个 confidential 与 70 个无许可文件。
本仓库因此**不宣称许可证发布闭环**：

- `project-tools` 归档只是**候选制品**：它证明「按权利边界生成的归档」在机械上可行且不含阻断
  内容，但**不能替代** GitHub 主分支的发布决定。
- **开放决策（需监督/权利方）**：GitHub 主分支是否作为发布目标？
  - 若否：公开发布流程须先把阻断路径从公开分支移除（本轮**不执行**历史重写/删除，仅记录决策点）；
  - 若是：须先解决 3 confidential + 70 无许可路径的权利链。
- `patches/`（驱动派生补丁，含未分类驱动源码片段）在任一情况下都不随 `project-tools` /
  `driver-source` 发布。

## 5. 机械审计与发布门禁

```sh
python3 tools/audit-licenses.py                                        # 机械一致性检查
python3 tools/audit-licenses.py --artifact project-tools --require-releasable
python3 tools/build-release-archive.py --artifact project-tools --out /tmp/project-tools.tar.gz
```

- 审计器只读 `.git`（`git ls-files --stage -z` / `git ls-tree` / `git cat-file` / `git status`），
  不写 index、不创建 index.lock。
- 保留的机械检查：许可证文本存在且 hash 固定、根许可证范围明确、上游 MIT notice 保留、
  drivers/ 分类（explicit/confidential/unclassified）、制品 allowlist 不含
  confidential/`NOASSERTION`/vendor/deb、allowlist 路径存在/唯一/属于目标提交、符号链接与路径
  穿越拒绝、脏树保护、确定性归档；**非 drivers/ 失败关闭逐路径分类**（`non_drivers_licenses`：
  已批准原创前缀 + 显式路径映射——上游 MIT、`LICENSES/` 标准文本组、`components/` 第三方派生组；
  未知路径一律拒绝 `non_drivers_unclassified`，**无全局默认 GPL**）+ **路径绑定 NOTICE 门禁**
  （`notice_gate.entries` 按路径组绑定版权/许可标记，缺失即失败；新第三方路径必须有对应条目）；
  `patches/`、`debs/` 整目录从 project-tools 排除（denylist 拒绝）。
- `status=CLEARED` **本身不是授权依据**：发布模式必须通过全部机械检查且工作树干净
  （工作区 == index == HEAD，policy/allowlist/文件 blob 均从 HEAD 读取）；`--draft` 只做结构
  测试并输出 `archive_draft=OK`，绝不输出 release PASS。
- SPDX 表达式只接受精确允许集合（`GPL-3.0-or-later`、`MIT`、`MIT OR GPL-2.0-only`、
  `BSD-3-Clause OR LGPL-2.1-only`、`GPL-2.0-only`、`BSD-3-Clause`、`LGPL-2.1-only`、
  `MPL-2.0`）；`NOASSERTION`、`vendor-binary` 与未决 `LicenseRef` 不作为可发布许可。

## 6. 维护要求

- 导入或修改 `drivers/`、`binary-manifest.json` 或许可证材料后：更新
  `license-audit-policy.json` 的允许集合与期望统计 → 运行
  `python3 tools/audit-licenses.py --write-inventory --write-allowlists` → 复审 diff →
  运行检查模式与 `bash tests/unit/run-license-audit-tests.sh`。
- 不删除已有版权声明；不重写 Git 历史；不得代表 Tim Hant 或其他贡献者换证。
- 发布制品必须通过对应 `--artifact <name> --require-releasable` 与构建器；本地构建/验证不等于
  获得公开再分发权。
