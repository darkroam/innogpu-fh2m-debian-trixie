# Third-Party Notices（第三方声明）

本文件说明随本仓库/制品分发的第三方内容及其适用许可。它不是授权声明，不授予任何第三方内容的
许可证，也不替代权利方文件。许可边界的唯一权威文档是
[docs/project/licensing.md](docs/project/licensing.md)。

## 1. 上游 fork 来源（MIT）

本仓库 fork 自 [timhant/innogpu-fh2m-debian-trixie](https://github.com/timhant/innogpu-fh2m-debian-trixie)
（fork 起点提交 `8be37ed`，2026-02-08，作者 Tim Hant）。Tim Hant 原始代码及其仍存在的实质性
派生内容继续保留以下 MIT 授权，随本仓库/制品分发时不得删除：

```text
MIT License

Copyright (c) 2026 Tim Hant

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 2. drivers/ 导入源码（逐文件声明）

`drivers/` 是导入的厂商驱动源码（来源：Deepin 原包
`innogpu-fh2m_20250421190503-debug_amd64.deb` 内 `usr/src/innogpu-kernel-2.2/`）。它**不适用**
根 [LICENSE](LICENSE) 的 GPL-3.0-or-later；每个文件保留并沿用其自身文件头的许可声明：

- 标为 `Dual MIT/GPLv2` 的文件按原声明分发，规范化映射为 `MIT OR GPL-2.0-only`；
- 标为 `BSD-3-Clause OR LGPL-2.1-only` 的文件保持原双许可；
- 标为 `Strictly Confidential` 的文件**不进入公开制品**（精确路径见
  [source-license-audit.md](docs/project/source-license-audit.md)）；
- 无许可声明的文件**不进入公开制品**，不因位于 `drivers/` 或仓库内而自动获得任何许可证；
- `MODULE_LICENSE(...)` 只是内核模块元数据，不替代文件许可。

## 3. components/（第三方组件派生内容，随 project-tools 发布）

`components/` 是第三方开源组件的**派生修改/配置**，按上游组件许可分发，不视为本项目原创层。
**固定上游版本与封存的许可材料**（随 `LICENSES/` 提供标准文本副本，审计器按路径组强制登记）：

- `components/fbterm/001-configurable-redraw-scrolling.patch`：修改 **fbterm（Debian 1.7-5，
  上游 https://code.google.com/archive/p/fbterm/ ）** 的派生补丁。
  版权：**Copyright (C) 2008 dragchan <zgchan317@gmail.com>**；许可：**GPL-2.0-only**（GPL-2.0
  全文见 `LICENSES/GPL-2.0-only.txt`）。上游许可依据：Debian `fbterm_1.7-5_copyright`（
  https://metadata.ftp-master.debian.org/changelogs/main/f/fbterm/fbterm_1.7-5_copyright ）。
- `components/picom/001-probe-explicit-uniform-location.patch`：修改 **picom（固定 commit
  `6d676824c457a933c52e3e92c5a1856466f90545`，https://github.com/yshui/picom ）** 的
  `src/backend/gl/gl_common.c`。该目标文件声明 `SPDX-License-Identifier: MPL-2.0` 与
  **Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>**；补丁按 **MPL-2.0** 分发，全文见
  `LICENSES/MPL-2.0.txt`。
- `components/picom/picom.conf` 是本项目维护的配置文件，属于原创层，按 **GPL-3.0-or-later** 分发。
- 上述路径的逐路径分类以 `license-audit-policy.json` 的 `non_drivers_licenses` 为机器权威；
  审计器的 NOTICE 门禁（notice_gate）按**路径组**绑定上述版权/许可标记，缺失即审计失败。

## 4. patches/（驱动层派生补丁，不随公开制品发布）

`patches/` 是驱动源码层的补丁文件，**不随 project-tools / driver-source 发布**：

- `patches/001-kernel-6.12-compat.patch` 继承自上游 fork（Tim Hant MIT），并含 darkroam 扩展；
- `patches/002-…-027` 是**混合/未决许可**内容：部分为内核驱动源码（GPL-2.0-only 语境）的
  派生修改，部分为 inventory 判定为无许可（unclassified）的驱动源码片段（如 `hal_power.c`、
  `Kbuild`、`compat_kernel6.h`、`inno_devfreq_gov.c` 等）——**不给未分类片段推定 GPL-2.0-only**；
  因此**整体排除**，不随任何公开制品发布；
- 它们在仓库中保留（仓库级发布仍 BLOCKED），未取得权利确认前不进入任何归档。

## 5. 本地载荷与 debs/（不随公开制品发布）

`debs/`（**整目录**，含 `debs/README.md`）、`vendor/`、`build/`、`third_party/` 与
`*.deb` 是用户本地取得/构建的载荷，**不随任何公开制品发布**。`binary-manifest.json` 中的
`vendor-binary` 是来源分类标记，不是许可证名称，也不授予再分发权。用户须自行从第三方（如
Deepin 官方渠道）取得原包并在本地使用；本项目不托管、不镜像、不自动下载该包。

## 6. GitHub 主分支发布面

本仓库 GitHub 主分支（main）本身公开分发全部跟踪路径，包括 3 个 `Strictly Confidential` 与
70 个无许可文件；**仓库级发布未闭环**。本声明随 `project-tools` 候选制品分发时只描述该制品
的实际内容；项目-tools 归档是**候选制品**，不构成许可证发布闭环。

## 7. 免责声明

- 本声明**不授予任何第三方内容的许可证**，也不替代权利方许可；第三方条款以权利方文件为准。
- 由用户本地提取并构建的完整驱动/包是否可再次公开分发，仍需单独的权利依据。
- 本项目原创层按 [LICENSE](LICENSE)（GPL-3.0-or-later）分发；此前按 MIT 发布的版本及副本
  继续保有原 MIT 授权，本文件不撤销既有授权。
