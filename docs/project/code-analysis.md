# 代码深度分析（4.0.0-i1 基线）

> 按 `~/5.md` 第一阶段产出（2026-08-21）。本文件分析当前 `main` + `4.0.0-i1` 的真实实现：
> 架构、构建/发布链、运行/恢复链、脚本质量（P0-P3）与黑盒/许可证边界。历史版本（patched-N）
> 仅作为回退基线与 oracle。结论标注证据等级：`CONFIRMED`（源码/脚本/清单）、`OBSERVED`（实机/
> 日志）、`INFERRED`（机制推断）、`UNVERIFIED`（未证实）。
>
> 全程约束：不安装、不切换模块、不修改设备配置、不重启；本文是离线分析，不构成验收证据。

## A. 架构与目录边界

### A1. 当前构建源：`drivers/`（CONFIRMED）

- Deepin 202504 DKMS 源码树导入（提交 `f1c5767`）+ 9 个补丁转换提交（patch-001/002/006/009/007/023/025/026/027，
  含 `7dc7e19` 清理），共 484 文件（464 C/H）。
- 子目录：`innogpu/`（DPU/GEM/DRM/HAL 源码）、`innosrvkm/`（PVR services + PDP/DP/HDMI 源码）、
  `innovpu/`、`innodma/`、`innosmmu/`、`innopmbus/`、`innopower/`（电源/调频源码）、
  `tools/`（gpu_info 等内核工具）。
- **关键结论**：当前 `drivers/` 是**直接编译的源码树**，不再依赖 patch 叠加（`CONFIRMED`：
  `build-innogpu-driver.sh` 只做 `cp -r drivers/. ` + 黑盒放置 + 确定性变换，无 `patch -pN`）。
- 黑盒对象（5 个 `.o_shipped`）**不在** `drivers/` 内（`CONFIRMED`：git ls-files 无
  .o_shipped/.o.cmd；check-docs 有边界守卫）。

### A2. 历史补丁、legacy wrapper 与新构建器关系（CONFIRMED）

| 对象 | 角色 | 状态 |
| --- | --- | --- |
| `patches/*.patch`（13） | 历史内核补丁原件 | provenance/回退复现，不叠加构建 |
| `components/picom|fbterm/` | 第三方组件补丁与配置 | 独立构建入口（`build-patched-*.sh`） |
| `build-deepin-coherent.sh` | legacy patched 构建器 | p27 oracle + check-docs 版本护栏，保留 |
| `build-innogpu-driver.sh` | **当前新架构构建器** | 4.0.0-i1 产出；`SOURCE_DATE_EPOCH` 必填 |
| `build-patchedNN-*.sh` | legacy 包装/护栏 | 停用护栏（p17-20）与历史候选包装 |

### A3. 黑盒载荷生命周期（CONFIRMED）

`binary-manifest.json`（192 项，5 类 kind，license 全部 `vendor-binary`）← 由
`tools/generate-binary-manifest.py` 从 pinned Deepin deb（SHA `b5a70e78…f6f5b2`）确定性生成 →
`scripts/extract-vendor-binaries.sh` 幂等提取到被忽略的 `vendor/` → 构建器按 manifest 装配进包。
`third_party/` = Deepin 解包区（gitignore 子目录）；`build/` = 构建输出（候选 deb 与 staging）；
`.build/` = 临时校验工作区。**载荷不入库，清单入库**（CONFIRMED）。

### A4. 所有权边界（CONFIRMED）

- Picom/fbterm：外部组件，本项目维护 patched 构建（components/ + scripts/build-patched-*）。
- dotconfig/xdisplay：独立仓库，本项目只维护设备钩子/会话接入/恢复命令（`CONFIRMED`：check-docs
  拒绝仓库内出现 xdisplay 引擎副本）。
- 音频：HDA 内置（Conexant SN6180）+ FH2M HDMI 音频 + PipeWire；本仓库维护修复脚本与接入。

## B. 构建与发布链（CONFIRMED，逐环节）

```text
Deepin deb(校验 SHA) → generate-binary-manifest(校验+生成) → validate-binary-manifest(schema)
 → extract-vendor-binaries(--check-only 门禁) → build-innogpu-driver.sh:
    [版本排序>patched-27] → [manifest check-only] → staging(drivers/ + vendor/kernel + gpupll 变换)
    → 离线 DKMS 编译(make KERNELDIR) → vermagic 校验 → 包装配(vendor payload 按 case-map、
    helpers、maintainer scripts、ld.so.conf) → [.o.cmd 守卫] → mtime 归一化
    → dpkg-deb --build → check-release-package → 可复现双构建 SHA 对比
 → compare-oracle-candidates.sh(8+module_symbols 项 vs patched-27)
```

- 门禁逐项核实：版本排序（`dpkg --compare-versions`）、manifest check-only 失败即停、
  `.o.cmd` 守卫、`SOURCE_DATE_EPOCH` 缺失即失败（P0 级保护，CONFIRMED）。
- 构建器引用核验：`build-innogpu-driver.sh` 引用的 `extract-vendor-binaries.sh`、
  `patch-gpupll-object.py`、`check-release-package.sh` 均存在且路径正确（CONFIRMED）。
- maintainer scripts：与 legacy 构建器字节一致（oracle maintainer_scripts=PASS，CONFIRMED）。
- 可复现性：epoch 1787342400 双构建 SHA `68aea6c0…` 逐字一致（OBSERVED）。

## C. 运行与恢复链（部分 OBSERVED，部分文档/脚本分析）

链路层级（必须区分，`~/5.md` 要求）：

| 层 | 触发 | 失败模式/日志 | 恢复 | 需重启 |
| --- | --- | --- | --- | --- |
| 1 安装成功 | apt/dpkg | 依赖/解包失败 → dpkg 半配置 | apt -f / 重装回退包 | 否 |
| 2 DKMS 构建 | postinst | 构建错误 → `/var/lib/dkms/…/build/make.log` | 修复后 dkms build | 否（模块未装） |
| 3 initramfs/depmod | postinst | 错误 → dpkg 输出 | update-initramfs -u | 否 |
| 4 模块加载（重启后） | boot | `dmesg` PVR/innogpu 错误 | modprobe 检查、回退 p27 | 已重启 |
| 5 DRM/fbdev 节点 | 模块加载 | 无 card0/renderD128/fb0 | repair-dri-nodes.sh | 否 |
| 6 Xorg/硬件 GL | Xorg 启动 | Xorg.0.log、llvmpipe 回退 | check-desktop-hwgl.sh 定位 | 否 |
| 7 桌面/合成 | dwm+Picom | Picom GLX 失败 | compositor 回退 xcompmgr | 否 |
| 8 显示切换 | xdisplay watch | RandR 布局异常 | 恢复钩子/命令 | 否 |
| 9 TTY/黑屏 | 任意 | 无登录/黑屏 | restore-tty1-login.sh、recovery.md 链 | 视情况 |

- 已验证实机证据（OBSERVED，Phase 4）：B1-B12、A1-A12 全 PASS；回退演练 4.0.0-i1→p27→4.0.0-i1。
- 恢复链：`4.0.0-i1 → patched-27 → … → patched-17 → patched-8`，首选
  `apt install --allow-downgrades`（CONFIRMED，文档 recovery.md）。
- 黑屏纪律：不热卸载活动显卡模块；SSH/TTY 收集日志后给恢复命令（CONFIRMED，文档与 5.md）。

## D. 脚本质量审查（P0-P3）

统计（67 个 scripts/*.sh）：61 个 `set -euo pipefail`；65 个含 `set -e[u]`；5 个无 `set -e`
（`phase4-baseline-capture.sh`(set -u)、`picom-session.sh`、`restore-dp1-mode-x11.sh`、
`verify-install-status.sh`(set -u -o pipefail)、`xdisplay-session.sh`——均设计为容错/只读）。
27 个含 sudo（安装/恢复类用户入口）；9 个 `mktemp`+trap 清理。

| 级别 | 问题 | 位置 | 影响与建议 |
| --- | --- | --- | --- |
| P1 | 部分脚本用 `/tmp` 硬编码且无 trap 清理（18 个引用 /tmp，仅 9 个 mktemp+trap） | 各构建/检查脚本 | 并发/中断残留；建议统一 `mktemp -d ${TMPDIR:-/tmp}` + trap |
| P2 | `check-deb-dkms-build.sh` 等把工作区放 `/tmp`（本会话曾因 /tmp 跨调用丢失踩坑） | check-deb-dkms-build.sh 等 | 长时间构建建议 `$ROOT/.build/`；短期可接受 |
| P2 | 5 个脚本无 `set -e` | 见上 | 容错设计可接受，但需文档说明；`verify-install-status.sh` 是只读报告器，符合设计 |
| P3 | 文档/脚本登记完整性由 check-docs 强制（scripts/README 全量登记，CONFIRMED 通过） | — | 无行动项 |

> 未发现 P0（数据破坏/提权/路径穿越/热切换）问题：提取器拒绝路径穿越、构建器原子 temp+rename、
> 恢复脚本不热卸载模块（CONFIRMED，此前 Phase 2/3 验证）。发现即记录，不冒充 PASS。

## E. 黑盒与许可证边界

- manifest 192 项：source_path/vendor_path/sha256/size/kind/role/license；5 类 kind
  （kernel-black-box/ddx/firmware/userspace-lib/userspace-config），license 全部 `vendor-binary`。
- `vendor-binary` = **来源分类，非许可证**（CONFIRMED：licensing.md 明文；不修改黑盒内容）。
- 已知许可证事实：源码树不是统一许可证。机械扫描发现 408 个文件引用来源分发中的
  `GPL-COPYING`/`MIT-COPYING`、3 个文件标为 `Strictly Confidential`、2 个文件声明
  BSD-3-Clause/LGPL-2.1-only；其余文件仍需逐项分类。缺失许可证文本、confidential 文件再分发权和
  用户态库/固件/DDX 逐项许可均为 UNVERIFIED，源码发布状态为 BLOCKED（见
  `source-license-audit.md`）；本仓库有权授权的原创辅助工作使用根 MIT。
- 黑盒对象不可读源码（`.o_shipped`），仅字节契约（gpupll 变换）与符号级可观察（OBSERVED）。

## 风险与优化候选

1. 构建环境依赖本机 `/lib/modules/$(uname -r)/build`（P2）：CI 无法跑完整 DKMS 构建，需
   linux-headers 预装；可增加"headers 缺失即清晰报错"的 fixture 测试。
2. 设备节点/TTY/X 相关验证只能实机（P2）：测试体系需把 runtime 与 offline 分层隔离（见
   test-strategy.md）。
3. 源码与载荷再分发许可未完成逐项核实（P0 发布阻断）：见 `source-license-audit.md`。
4. 黑盒符号级分析未完成（远期候选 9）：依赖可观察符号，不修改黑盒。
5. 能力深挖（DVFS/codec 编码/CORE_ID）为未实施项（见 todo.md），非本阶段范围。
6. 参考模型版本漂移风险：上游 Picom commit 固定 `6d676824`（CONFIRMED）；Deepin 包 SHA 固定；
   DRM/Mesa 主线仅作行为对照（INFERRED 用途，见 frameworks-and-references.md）。

## 证据索引

- 构建链：`scripts/build-innogpu-driver.sh`、`compare-oracle-candidates.sh`、`extract-vendor-binaries.sh`
- 运行链：`build-innogpu-driver.sh` postinst 段、`docs/user/recovery.md`、`docs/planning/phase4-device-validation.md`
- 边界：`binary-manifest.json`、`docs/project/licensing.md`、`maintenance-policy.md`
- 测试：`tests/README.md`、`scripts/check-docs.sh`
