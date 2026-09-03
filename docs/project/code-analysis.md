# 代码深度分析（4.0.0-i1 基线）

> 2026-08-21 首次产出。本文件分析当前 `main` + `4.0.0-i1` 的真实实现：
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
- **关键结论**：`4.0.0-i1` 的 `drivers/` 是**直接编译的源码树**，不依赖历史 patch 叠加；
  失败候选 `4.0.1-i1` 额外应用 patch-024（接线 `CONFIRMED`、s2idle 可见恢复验收失败）。
  `4.0.1-i2` 在其后再应用 patch-025-suspend-resume-display，R05 单次 s2idle 恢复通过；R06
  使用 i3/i4 同 epoch 包级单变量对照继续定位（准备 `CONFIRMED`，因果仍 `UNVERIFIED`）。R10/R11
  deep 先后证明 patch-024 与 lifecycle 026 仍未覆盖全部 PreClock 入口；R12 `4.0.2-i2` 增加
  patch-028，在 PVR 子设备与 DVFS 恢复成功后才启动独立温度 work；R13 `4.0.2-i3` 增加
  patch-029，让 DDCCI 回退模式创建 panel 但不注册 backlight device（静态/离线
  `CONFIRMED`，真机 `UNVERIFIED`）。
- 黑盒对象（5 个 `.o_shipped`）**不在** `drivers/` 内（`CONFIRMED`：git ls-files 无
  .o_shipped/.o.cmd；check-docs 有边界守卫）。

### A2. 历史补丁、legacy wrapper 与新构建器关系（CONFIRMED）

| 对象 | 角色 | 状态 |
| --- | --- | --- |
| `patches/*.patch`（18） | 13 个历史内核补丁原件 + patch-024/025-display/026-lifecycle/028-temp-monitor/029-ddcci-panel 行为候选 | 历史项仅作 provenance/回退复现；行为候选按版本接入新构建器 |
| `components/picom|fbterm/` | 第三方组件补丁与配置 | 独立构建入口（`build-patched-*.sh`） |
| `build-deepin-coherent.sh` | legacy patched 构建器 | p27 oracle + check-docs 版本护栏，保留 |
| `build-innogpu-driver.sh` | **当前新架构构建器** | 默认 i3 绑定 024+026+028+029；i1/i2 与 R06 i3/i4 仅供历史复现；拒绝未审版本/epoch 和 patch 备份/reject 产物 |
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

链路层级（安装、构建、加载和运行证据必须区分）：

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
- 黑屏纪律：不热卸载活动显卡模块；SSH/TTY 收集日志后给恢复命令（CONFIRMED，见维护与恢复文档）。

## D. 脚本质量审查（P0-P3）

统计（67 个 scripts/*.sh）：61 个 `set -euo pipefail`；65 个含 `set -e[u]`；5 个无 `set -e`
（`phase4-baseline-capture.sh`(set -u)、`picom-session.sh`、`restore-dp1-mode-x11.sh`、
`verify-install-status.sh`(set -u -o pipefail)、`xdisplay-session.sh`——均设计为容错/只读）。
27 个含 sudo（安装/恢复类用户入口）；9 个 `mktemp`+trap 清理。

| 级别 | 问题 | 位置 | 影响与建议 |
| --- | --- | --- | --- |
| P1 | 部分脚本用 `/tmp` 硬编码且无 trap 清理（20 个脚本包含 `/tmp`，仅 9 个脚本使用 mktemp+trap） | 各构建/检查脚本 | 并发/中断残留；建议统一 `mktemp -d ${TMPDIR:-/tmp}` + trap |
| P2 | `check-deb-dkms-build.sh` 等把工作区放 `/tmp`（本会话曾因 /tmp 跨调用丢失踩坑） | check-deb-dkms-build.sh 等 | 长时间构建建议 `$ROOT/.build/`；短期可接受 |
| P2 | 5 个脚本无 `set -e` | 见上 | 容错设计可接受，但需文档说明；`verify-install-status.sh` 是只读报告器，符合设计 |
| ~~P1~~ 已修复 | DRI repair 源码 fallback 与 unit 路径不一致、启动失败被忽略、卸载不清理 fallback helper | `install-dri-node-repair-service.sh`、`uninstall-innogpu.sh` | 2026-08-28 已修复：helper 三态判定（absent/owned/foreign，外国文件拒绝覆盖）、unit `ExecStart` 指向实际安装的 helper（包 `/usr/sbin` vs 源码 fallback `/usr/local/sbin` 明确区分）、失败回滚只删本次新建、`enable/start` 失败传播、卸载器只删规范化目标精确等于本仓库脚本的符号链接且版本不匹配零副作用、package-absent 只清 DRI 自有路径、失败重装不改已有启用状态且原样恢复既有 unit、卸载器只删带项目 marker 的 unit；fixture 见 `tests/unit/run-dri-repair-tests.sh` |
| ~~P1~~ 已修复 | 两个历史/诊断脚本把目标用户默认写死为 `ok`，`check-soft-xorg-dwm.sh` 用户与 home 推导不一致 | `check-soft-xorg-dwm.sh`、`try-hotload-patched17.sh` | 2026-08-28 已修复：`check-soft-xorg-dwm.sh` 解析顺序 `INNOGPU_X_USER > SUDO_USER > USER`、home 用 `INNOGPU_X_HOME`/`getent` 且不可确定时明确失败（不回退 root `$HOME`）、`run_x` 用同一解析用户；`try-hotload-patched17.sh` 提示改中性；静态反例断言无硬编码用户名 |
| ~~P2~~ 已修复 | VA-API runner 只把 GNU timeout rc=124 识别为超时，rc=137 误归普通失败 | `run-vaapi-decode-test.sh` | 2026-08-28 已修复：三个阶段统一 `timeout_rc`（124/137 → 整体退出码 5）；`tests/unit/run-vaapi-decode-tests.sh` 增加忽略 TERM→SIGKILL fixture |
| P2 | 音频安装器创建系统/用户 unit、helper 和配置并修改旧用户配置，但没有对称卸载入口或 fixture；多条写入无原文件备份，未运行 `systemd-analyze verify`，用户服务管理失败被忽略 | `install-hygon-hda-audio.sh` | 当前只能人工审阅有限备份后回退；需补冲突检测、幂等卸载、所有权/备份恢复及 systemd fixture |
| ~~P2~~ 已修复 | `check-docs.sh` 的 Markdown 链接与隐私扫描目录集合曾未覆盖全部受跟踪文档 | `check-docs.sh` | 2026-08-31 已修复：链接从 `git ls-files -z -- '*.md'` 枚举，隐私扫描覆盖 tracked 文档目录和本机 `collab/`，两个消费者共用 `tools/private-data-patterns.txt`；内联路径语义仍需人工审查 |
| P2 | 包导入 `sw-inno-gl.service`/helper，但 control 无 `systemd` 依赖，maintainer scripts 不管理单元；release gate 也未校验该组合和全部 10 个 `/usr/bin`+`/usr/sbin` 命令链接 | `build-innogpu-driver.sh`、`check-release-package.sh` | 文件存在不等于服务启用；需决定保留/移除策略，再补依赖、生命周期和包边界 fixture |
| P3 | 文档/脚本登记完整性由 check-docs 强制（scripts/README 全量登记，CONFIRMED 通过） | — | 无行动项 |

> 未发现 P0（数据破坏/提权/路径穿越/热切换）问题：提取器拒绝路径穿越、构建器原子 temp+rename、
> 恢复脚本不热卸载模块（CONFIRMED，此前 Phase 2/3 验证）。发现即记录，不冒充 PASS。

## E. 黑盒与许可证边界

- manifest 192 项：source_path/vendor_path/sha256/size/kind/role/license；5 类 kind
  （kernel-black-box/ddx/firmware/userspace-lib/userspace-config），license 全部 `vendor-binary`。
- `vendor-binary` = **来源分类，非许可证**（CONFIRMED：licensing.md 明文；不修改黑盒内容）。
- 已知许可证事实：源码树不是统一许可证。逐文件 inventory 将 483 个导入实现/构建文件分类为
  408 个 `MIT OR GPL-2.0-only`、3 个 `Strictly Confidential`、2 个
  `BSD-3-Clause OR LGPL-2.1-only` 和 70 个 `NOASSERTION`；标准条款副本已补齐，但 confidential、
  未分类源码及用户态库/固件/DDX 的权利链仍为 UNVERIFIED，发布状态为 BLOCKED（见
  `source-license-audit.md`）。根 [LICENSE](../../LICENSE)（GPL-3.0-or-later）只覆盖本项目有权
  授权的原创层；`drivers/` 不适用根许可证，confidential 与无许可路径排除出公开制品。
- 黑盒对象不可读源码（`.o_shipped`），仅字节契约（gpupll 变换）与符号级可观察（OBSERVED）。

## 风险与优化候选

1. 构建环境依赖本机 `/lib/modules/$(uname -r)/build`（P2）：CI 无法跑完整 DKMS 构建，需
   linux-headers 预装；可增加"headers 缺失即清晰报错"的 fixture 测试。
2. 设备节点/TTY/X 相关验证只能实机（P2）：测试体系需把 runtime 与 offline 分层隔离（见
   test-strategy.md）。
3. 源码与载荷再分发许可未完成逐项核实（P0 发布阻断）：见 `source-license-audit.md`。
4. 黑盒符号级分析未完成（远期候选 9）：依赖可观察符号，不修改黑盒。
5. 能力深挖（DVFS/codec 编码/CORE_ID）为未实施项（见 `../planning/current-work.md`），非本阶段范围。
6. 运维与包生命周期闭环：DRI service 路径/失败传播、固定用户名、VA-API rc=137 三项已于
   2026-08-28 修复并加 fixture（见上表）；check-docs 全量 Markdown/隐私覆盖已于 2026-08-31 完成；音频卸载、构建依赖门禁以及 vendor
   systemd/helper 包边界缺口保留为后续批次。
7. 参考模型版本漂移风险：上游 Picom commit 固定 `6d676824`（CONFIRMED）；Deepin 包 SHA 固定；
   DRM/Mesa 主线仅作行为对照（INFERRED 用途，见 frameworks-and-references.md）。

## 证据索引

- 构建链：`scripts/build-innogpu-driver.sh`、`compare-oracle-candidates.sh`、`extract-vendor-binaries.sh`
- 运行链：`build-innogpu-driver.sh` postinst 段、`docs/user/recovery.md`、`docs/planning/phase4-device-validation.md`
- 边界：`binary-manifest.json`、`docs/project/licensing.md`、`maintenance-policy.md`
- 测试：`tests/README.md`、`scripts/check-docs.sh`
