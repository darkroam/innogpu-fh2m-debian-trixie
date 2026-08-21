# 源码树迁移设计（Phase 0 设计冻结）

## 状态

- 本文件是源码树迁移的设计基线，对应监督指南 `docs/planning/migration-supervision.md`（监督分支
  migration/supervised-source-tree @ bd76e91）。
- **阶段 0（设计冻结）✅ 阶段 1（源码树导入 + 9 补丁转提交 + parity）✅ 阶段 2（manifest + 幂等提取 +
  staging 内核编译）✅**；当前进入阶段 3（新构建器并行验证）申请。
- 当前设备基线：`3.3.3.42-patched-27`（迁移冻结运行基线与回退包）。
- 未实机验证的内容一律标记 UNVERIFIED；本文件所有行为承诺均需在对应阶段用命令和输出证明。
- **边界声明**：阶段 2 的 staging 构建只装配 5 个内核黑盒对象并编译 DKMS；用户态库、固件与 ALSA UCM
  载荷的**包边界验证属于阶段 3**，当前 staging 结果不代表完整驱动包验证。

## 一、迁移目标（对齐监督指南"二、迁移目标"）

```text
Git 跟踪：        可维护 C/H 驱动源码（drivers/）
Git 忽略：        vendor 黑盒对象、用户态库、固件、构建产物（vendor/ build/）
manifest：        外部载荷来源、路径、角色、大小和 SHA-256（binary-manifest.json）
staging：         源码与黑盒载荷的隔离构建树（build/<unique>/source）
release：         独立版本、源码提交、manifest hash 和 parity report
```

"取消 patch" 只表示源码改动最终直接存在于源码树中；**不表示立即删除历史 patch，不表示黑盒
驱动核心已经源码化**。在阶段 1–5 全部 PASS 前，`patches/`、旧 wrapper、p27 deb 与 p27 tag
永久保留。

## 二、目标目录结构

```text
drivers/                         Git 跟踪的可维护 DKMS 源码树
  innogpu/ innovpu/ innodma/ innosmmu/ innopmbus/ innopower/ innosrvkm/
  Makefile Kbuild dkms.conf modules_config.sh ...
vendor/                          Git 忽略的黑盒就位区（由提取工具填充）
  kernel/innogpu/innogpu.o_shipped
  kernel/innodma/innodma.o_shipped
  kernel/innosrvkm/innosrvkm.o_shipped
  kernel/innovpu/innovpu.o_shipped
  kernel/innosmmu/innosmmu.o_shipped
  userspace/...（18 个 .so + DDX/GBM/DRI）
  firmware/...（fh2m.fw/fh2m.sh/fh2c.fw/fh2c.sh）
build/                           Git 忽略的临时 staging 与构建产物
binary-manifest.json             黑盒来源、路径、哈希、大小、类型、角色和许可证的唯一清单
scripts/
  extract-vendor-binaries.sh     幂等提取工具
  build-innogpu-driver.sh        新构建器（不执行 patch -pN）
  run-dev-tests.sh               开发测试闭环
tests/kernel/                    内核离线编译与探针回归测试
docs/project/driver-architecture.md
docs/user/userspace-components.md
legacy/                          迁移完成后保存旧 patch/wrapper 的位置（阶段 5 才创建）
patches/                         迁移完成前保留，不得提前删除
```

## 三、14 个 patch 的 provenance 与分类

分类四类（监督指南"三、patch 分类规则"）：`source`（源码提交）、`binary-transform`
（确定性二进制变换）、`device-profile`（本机特例）、`closed`（关闭的历史试验）。
启用状态以 patched-27 的开关集合为准。patch hash 为当前 `patches/` 文件 SHA-256；stage-000
无 patch 文件，是工具 `tools/patch-gpupll-object.py`。

| # | 类别 | 启用 | patch SHA-256 | 目标 | 转换计划 |
| --- | --- | --- | --- | --- | --- |
| 000 | binary-transform | 始终 | （工具）`tools/patch-gpupll-object.py` | `innogpu.o_shipped` 单点字节变换 | 保留为独立确定性工具，输入/输出 hash 入清单；不做成源码提交 |
| 001 | source | 始终 | `be5c8ae9...71ab5` | 多文件 6.12 兼容 + Kbuild `-Wno-error` | 拆分为源码提交；Kbuild 改动归 build-metadata |
| 002 | source | 是 | `1a12de65...7329` | DP fbcon fallback | 源码提交 |
| 003 | closed | 否 | `8cd6b492...c6f7b` | 背光试验 | 仅历史记录，不导入当前行为 |
| 004 | closed | 否 | `330c3a06...4513` | 平台试验 | 仅历史记录 |
| 005 | closed | 否 | `9fee230c...ec15` | 背光试验 | 仅历史记录 |
| 006 | device-profile | 是 | `63a6569c...40b5` | 本机 connector/ACPI 映射 | 进入 device profile / 设备适配层，保留明确边界 |
| 007 | source | 是 | `1adb7a37...7733` | fbdev io mmap | 源码提交 |
| 008 | closed | 否 | `4cfd545a...6394` | PVR 诊断 | 仅历史记录 |
| 009 | device-profile | 是 | `e6b955fd...b26c` | 本机内置 eDP/connector | 进入 device profile |
| 023 | source | 是 | `ea35a852...01f63` | invisible GEM 不回写 | 源码提交 |
| 025 | source | 是 | `05de1bdd...a027` | dma_resv usage | 源码提交 |
| 026 | source | 是 | `864bc3d6...216b` | vblank 守卫 | 源码提交 |
| 027 | source | 是 | `ab2d1b41...7ca5` | foreign DMA-BUF 生命周期 | 源码提交 |

转换提交规则：commit message 保留原编号（如 `source: patch-025 dma_resv usage semantics`），
body 引用 `docs/patches/patch-*.md`；记录原 patch hash、目标文件、转换后提交 hash 与行为变化。

## 四、binary-manifest.json 正式 schema（监督指南 + 5.md 要求）

正式 JSON，无注释、无省略号；覆盖全部 `.o_shipped`、用户态 `.so`、DDX/GBM/DRI 与固件：

```json
{
  "format_version": 1,
  "source_package": "innogpu-fh2m",
  "source_version": "20250421190503-debug",
  "source_deb_sha256": "<Deepin deb SHA-256>",
  "architecture": "amd64",
  "entries": [
    {
      "source_path": "usr/src/innogpu-kernel-2.2/innogpu/innogpu.o_shipped",
      "vendor_path": "kernel/innogpu/innogpu.o_shipped",
      "sha256": "<文件 SHA-256>",
      "size": 0,
      "kind": "kernel-black-box",
      "role": "FH2M HAL",
      "license": "vendor-binary"
    }
  ]
}
```

`kind` 取值集合：`kernel-black-box`、`userspace-lib`、`ddx`、`firmware`。
校验拒绝：路径穿越、重复目标、未知 kind、源包哈希错误、原子替换失败。

## 五、幂等提取工具规范（`scripts/extract-vendor-binaries.sh`）

- 默认从 `debs/` 找 Deepin 原包，支持 `INNOGPU_DEEPIN_DEB` 环境变量；
- 先校验原 deb SHA-256 与清单一致；
- 用 `dpkg-deb --extract` 或等价结构化方式解出，不用脆弱字符串截取；
- 目标存在且哈希正确 → 跳过（幂等）；缺失/不匹配 → 安全重建；
- 检测路径穿越、重复目标、未知 kind、哈希错误；
- `--check-only` 只读不写，输出每项就位/缺失/不符状态；
- 临时目录 + 原子 rename，中途失败不留伪完整文件；
- 输出机器可读 PASS/FAIL 摘要；二次执行必须证明幂等。

## 六、staging 构建树与新构建器（`scripts/build-innogpu-driver.sh`）

构建流程（不执行 `patch -pN`）：

```text
验证 Deepin 原包
  -> 验证 binary-manifest.json
  -> extract-vendor-binaries.sh --check-only
  -> 创建 build/<unique>/source
  -> 导入 drivers/ 源码
  -> 放置已校验 vendor 黑盒对象
  -> 执行确定性 binary transform（patch-000 等价工具）
  -> 编译 DKMS
  -> 包装同源用户态/固件/maintainer scripts
  -> package boundary audit
  -> 输出 parity report
```

禁止：以历史 patched deb 为输入；自动下载未固定哈希的外部文件；修改 Git 工作区源码；
从个人 home 目录读取黑盒载荷；编译失败时复制旧 .ko 绕过。`drivers/` 中不放临时 `.o_shipped`，
不依赖人工软链接。

## 七、parity 验证门槛（监督指南"五、强制 parity 报告"）

每个阶段必须输出机器可读报告，至少包含：

```text
source_tree_parity=PASS
patch_provenance=PASS
binary_manifest=PASS
vendor_extraction_idempotent=PASS
deterministic_transform=PASS
dkms_build=PASS
module_vermagic=PASS
package_boundary=PASS
userspace_firmware_coherence=PASS
runtime_probe=PASS
rollback=PASS
```

区分三类树：Deepin 原始源码树、Deepin 原包 + p27 有效源码修改后的生成树、迁移后的
`drivers/` 源码树。源码 parity 只比较源码生成树；黑盒/用户态/固件/maintainer scripts 分别由
binary、package、runtime parity 验证。禁止用单项编译成功替代完整 parity。

## 八、版本、tag 与发布

- 新架构首版：`Debian package 1.0.0-i1`、`Git tag source-v1.0.0-i1`，不覆盖 `patched-27`；
- tag 注释含：源码提交 hash、binary-manifest hash、Deepin 原包 hash、构建工具版本、parity report
  位置、实机验证状态、p27 回退包位置；
- **必须先验证 Debian 版本排序**：用 `dpkg --compare-versions` 实际验证
  `1.0.0-i1` 与 `3.3.3.42-patched-27` 的升级/降级/回退关系，不能只在文档中假定；
- 旧 tag 不得移动；新架构用新 tag；p27 tag 与 release 永久保留。

## 九、分阶段计划（对齐监督指南"四、分阶段门槛"）

| 阶段 | 交付 | 门槛（全部 PASS 才进下一阶段） |
| --- | --- | --- |
| 0 设计冻结 | 本文件：目录、manifest schema、提取工具、staging、版本排序、许可证、回退策略 | 设计审查通过；不改设备不删旧文件 |
| 1 源码树导入 | `drivers/`、源码架构说明、patch provenance 表、导入报告 | source_import / patch_provenance / source_tree_parity_against_p27 / working_tree_clean / runtime_unchanged |
| 2 黑盒 manifest 与 staging | 正式 manifest、提取工具、vendor 忽略规则、staging 构建 | first_extraction / second_extraction_idempotent / check_only / bad_hash=FAIL_AS_EXPECTED / missing_source=FAIL_AS_EXPECTED / path_traversal=FAIL_AS_EXPECTED / interrupted_extraction_recovery |
| 3 新构建器并行验证 | 新构建器（旧构建器为 oracle，并行比较） | 源码/黑盒/用户态/固件/maintainer scripts/包清单/vermagic/关键符号逐项一致 |
| 4 实机候选验证 | 安装候选（需监督批准 + p27 回退与 SSH/TTY 通道） | 包版本/DKMS/vermagic/Driver/Firmware/DRM/fbdev/HWGL/DRI3/PDP/vblank/VA-API/fbterm/xdisplay/Picom/音频 + p27 回退演练 |
| 5 旧流程退役 | `patches/` 与旧 wrapper 移入 `legacy/` 或标记 deprecated | 阶段 1–4 全部 PASS + 新设备 clone 安装验证；p27 tag/deb 永久保留；至少一个发布周期后才评估物理删除 |

## 十、暂停条件（监督指南"八、暂停条件"）

manifest 哈希与来源包不一致、新旧源码树无法解释地不同、新构建器修改 Git 工作区、编译失败却
复制旧 .ko 绕过、包版本排序未验证、用户态/固件来源混用、实机验证缺回退通道、文档把计划写成
实机已验证、p27 回退路径未保留、需要强制移动既有 tag —— 任一出现立即停止并报告。

## 十一、agent 协作与交付格式（监督指南"七"）

开工声明：当前分支、基准提交、允许修改的文件、不允许修改的文件和系统状态、测试与回退方式。
收工交付：修改文件清单、设计决策与未解决问题、测试命令及 PASS/FAIL 摘要、parity report、
是否需要重启、是否允许合并 main。涉及设备安装/模块切换/重启/tag 移动/删除旧流程时必须暂停
等待监督确认。无法验证的内容标记 `UNVERIFIED`。

## 参考

- `docs/planning/migration-supervision.md`（监督分支 migration/supervised-source-tree @ bd76e91）：监督指南（阶段门槛与暂停条件优先）。
- [ddk-v119-mapping.md](ddk-v119-mapping.md)、[release-review-2026-08-20.md](release-review-2026-08-20.md)。
