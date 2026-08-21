# 源码树迁移监督指南

## 状态与所有权

本文是源码树迁移的监督基线和本分支工作指南，不是迁移实现本身。

- 所属分支：`migration/supervised-source-tree`
- 当前设备基线：`3.3.3.42-patched-27`
- 当前迁移状态：设计审查，尚未实施源码树迁移
- 文档维护者：本监督分支的维护 agent
- 其他 agent 不得直接修改本文；如需调整，必须先提出评审意见并由监督分支维护者合并

本指南用于监督 dsh 及其他 agent 的后续工作，防止把计划、离线验证和实机验证混写。

## 一、不可变边界

迁移期间必须保持以下内容不变：

1. 不删除或修改 `main`、`patched-27` tag、p27 release 记录和 p27 回退 deb。
2. 不提前删除 `patches/`、旧 wrapper 或旧构建流程。
3. 不安装迁移中的候选包，不热切换显卡模块，不因设计工作要求重启。
4. 不从历史 patched 包复制 `.so`、固件、`.ko` 或 maintainer script。
5. Deepin `20250421190503-debug` 原包仍是唯一技术基线。
6. 所有黑盒 `.o_shipped`、闭源 `.so` 和固件必须由哈希清单管理，不进入 Git。
7. 任何无法验证的内容必须标记为 `UNVERIFIED`，不得写成完成。

## 二、迁移目标

目标架构为：

```text
Git 跟踪：        可维护 C/H 驱动源码
Git 忽略：        vendor 黑盒对象、用户态库、固件、构建产物
manifest：        外部载荷来源、路径、角色、大小和 SHA-256
staging：         源码与黑盒载荷的隔离构建树
release：         独立版本、源码提交、manifest hash 和 parity report
```

“取消 patch”只表示源码改动最终直接存在源码树中，不表示立即删除历史 patch，也不表示黑盒驱动核心已经源码化。

## 三、patch 分类规则

现有 patch 共 14 个：

```text
000  二进制确定性变换
001  内核兼容源码
002  DP fbcon 源码
003  历史背光试验
004  历史平台试验
005  历史背光试验
006  本机 connector 源码特例
007  fbdev mmap 源码
008  历史诊断
009  本机 eDP/connector 源码特例
023  invisible GEM 源码
025  dma_resv usage 源码
026  inactive CRTC vblank 源码
027  foreign DMA-BUF 生命周期源码
```

转换时必须区分：

- 普通 C/H 修改：转换为 `drivers/` 中的源码提交；
- patch-000：继续使用独立的确定性二进制变换工具；
- 本机特例：进入明确的 device profile 或设备适配层；
- 关闭的历史试验：保留历史文档，不自动导入当前行为；
- 用户态、固件和 maintainer scripts：进入 manifest/打包边界，不伪装成源码。

每个转换提交必须保留原 patch 编号、原 patch hash、目标文件、对应文档链接和验证结果。

## 四、分阶段门槛

### 阶段 0：设计冻结

必须先完成并审查：

- 目标目录结构；
- manifest 正式 schema；
- vendor 与 staging 接入方式；
- Debian 包版本排序策略；
- 许可证和黑盒分发边界；
- 旧流程和回退策略。

阶段 0 不得修改设备状态，不得删除旧文件。

### 阶段 1：源码树导入

交付：`drivers/`、源码架构说明、patch provenance 表和导入报告。

门槛：

```text
source_import=PASS
patch_provenance=PASS
source_tree_parity_against_p27=PASS
working_tree_clean=PASS
runtime_unchanged=PASS
```

此阶段不改旧构建器，不生成新 release，不安装新包。

### 阶段 2：黑盒 manifest 与 staging

交付：正式 `binary-manifest.json`、提取工具、vendor 忽略规则和 staging 构建工具。

必须验证：

```text
first_extraction=PASS
second_extraction_idempotent=PASS
check_only=PASS
bad_hash=FAIL_AS_EXPECTED
missing_source=FAIL_AS_EXPECTED
path_traversal=FAIL_AS_EXPECTED
interrupted_extraction_recovery=PASS
```

提取工具必须使用临时目录和原子 rename，不得把半成品留在 vendor 中。

### 阶段 3：新构建器并行验证

旧构建器是 oracle，新构建器只能并行运行：

```text
old builder -> p27 reference
new builder -> migration candidate
```

必须比较源码树、黑盒文件、用户态库、固件、maintainer scripts、包清单、模块 vermagic 和关键符号。

### 阶段 4：实机候选验证

只有阶段 1 至 3 全部通过，才可申请安装。申请必须附带 p27 回退包、SSH/真实 TTY 恢复路径和明确重启说明。

实机至少验证：

- 包版本、DKMS、模块 vermagic；
- Driver/Firmware、DRM/fbdev；
- Xorg/dwm、硬件 GL、DRI3/PRIME；
- PDP READ/WRITE、vblank、VA-API；
- fbterm、xdisplay、Picom、音频；
- 回退到 p27 并恢复正常登录。

### 阶段 5：旧流程退役

只有新架构完成离线验证、实机验证、回退演练和新设备 clone 安装验证后，才允许把旧 patch/wrapper 移到 `legacy/` 或标记 deprecated。

旧 tag 和 p27 release 永久保留，至少经过一个完整发布周期后才评估物理删除旧文件。

## 五、强制 parity 报告

每个迁移阶段必须提供机器可读报告，至少包含：

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

禁止用单项编译成功替代完整 parity；禁止用隔离 Xorg 结果宣称真实桌面验证通过。

## 六、版本与 tag 规则

新架构不得复用或移动 `patched-*` tag。必须先解决 Debian 版本排序，再定义独立版本，例如：

```text
Debian package: 1.0.0-i1
Git tag: source-v1.0.0-i1
```

tag 注释必须包含源码提交、manifest hash、Deepin 原包 hash、构建工具版本、parity report 和回退包。

## 七、agent 协作规则

每个 agent 开工前必须说明：

1. 当前分支和基准提交；
2. 本阶段允许修改的文件；
3. 不允许修改的文件和系统状态；
4. 预计测试和回退方式。

每个 agent 收工时必须提供：

1. 修改文件清单；
2. 设计决策和未解决问题；
3. 测试命令及 PASS/FAIL 摘要；
4. parity report；
5. 是否需要重启；
6. 是否允许合并到 `main`。

涉及设备安装、模块切换、重启、tag 移动或删除旧流程时，必须暂停并等待监督确认。

## 八、暂停条件

出现以下任一情况，立即停止迁移并报告，不得继续推进：

- manifest 哈希与来源包不一致；
- 新旧源码树无法解释地不同；
- 新构建器修改 Git 工作区；
- DKMS 编译失败但有人提出复制旧 `.ko` 绕过；
- 包版本排序未验证；
- 用户态或固件来源混用；
- 实机验证缺少 SSH/TTY 回退通道；
- 文档把计划或离线结果写成实机已验证；
- p27 回退路径未保留；
- 需要强制移动既有 tag 才能继续。

## 九、监督结论

当前仅批准继续完善阶段 0 设计。未批准：

- 删除 `patches/`；
- 删除旧 wrapper；
- 修改当前设备驱动；
- 安装新架构候选；
- 重启设备；
- 发布新的独立版本。

阶段 0 完成后，必须重新评审，才能批准阶段 1 的源码树导入。
