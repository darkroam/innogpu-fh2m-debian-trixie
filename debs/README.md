# Release 包目录

`debs/` 是**本地存档/构建/回退目录**：存放 Deepin 原包与本地构建产物。目录中的 `.deb` 文件
全部不进入 Git，也**不随公开制品发布**；任何公开分发必须单独通过发布审阅与对应制品门禁
（`project-tools` / `driver-source`，见 [docs/project/licensing.md](../docs/project/licensing.md)）。
本目录存在不等于可发布。仓库只跟踪本说明文件，避免把大体积二进制包和本机版本状态混入源码历史。

## 输入包

后续 coherent 构建以 Deepin 202504 原包为唯一技术基线。**当前新架构（4.0.0-i1）**由
`scripts/build-innogpu-driver.sh` 驱动（drivers/ 源码树 + `binary-manifest.json` 黑盒载荷 + 确定性
变换，产出到被忽略的 `build/`，可复现 epoch 1787342400）；`scripts/build-deepin-coherent.sh` 为
legacy patched 系构建器（保留作 p27 oracle 与版本护栏）。两者都以本目录的 Deepin 原包为输入：

```text
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

新架构构建器经 manifest/提取器默认只读取上述 `debs/` 路径；其他位置必须显式设置
`INNOGPU_DEEPIN_DEB`。`scripts/build-deepin-coherent.sh` 与
`scripts/prepare-deepin-userspace-root.sh` 仍保留仓库根旧路径兼容查找，但它只服务 legacy 流程。
历史 patched 包只用于安装回退或复现记录，不能作为新包输入。

本机保存的 patched-19/20 deb 生成于 xdisplay 所有权收敛之前，包内仍有旧显示引擎和实验辅助文件。
它们只能作为历史机器证据，不得上传为当前 release。当前源码禁止复用 19/20 版本号。

当前本地 p21 输出已通过包边界和两次逐字一致构建，并已在当前设备完成部署、重启和运行验收；实际身份
记录在 [`patched-21-release-candidate.md`](../docs/patches/patched-21-release-candidate.md)。这不等于它
已完成跨硬件 release：维护者不得仅凭文件位于 `debs/` 或当前设备通过就上传 release。

当前本地 p22 输出已通过包边界检查，并已在当前设备完成安装、重启和 patch-009 connector/桌面烟测；
其 SHA-256、限制条件和未完成的电源/合盖矩阵见 [`patch-009-local-internal-edp-connector.md`](../docs/patches/patch-009-local-internal-edp-connector.md)。

## 输出包

### patched-17 文件边界

patched-17 只保留以下 canonical 回退包：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-17.deb
```

同版本的 `*_patched-17_amd64.deb` 是本机 `dpkg-repack` 生成的重打包副本，
包内容和校验值不同，不属于已演练的回退产物，已从工作目录清除。安装脚本、回退文档
和 `patched-17` tag 均只引用上面的 canonical 文件。

## Git Tag 与包对应

每个可追溯的 deb 使用同名 annotated tag 标识其源码提交和 SHA-256。tag 不包含 deb 文件，发布时仍
从本地 `debs/` 或 release 附件取得二进制：

| 包 | Git tag | SHA-256 |
| --- | --- | --- |
| patched-17 | `patched-17` | `51e9bcbb074734b7f3218c0ad0882e3c4a069187524e66799af1571544a7f853` |
| patched-21 | `patched-21` | `15c1fab4b8a0f36985097e3d1651ff43fc09f00a2bda47058c380e1e561384cc` |
| patched-22 | `patched-22` | `aae8f966af7c5737037869a4e6ee5d081fd07d386dec67e0e799746ff6386ae9` |
| patched-23 | `patched-23` | `da1479f6264406443616f342e917b7f95d5798c98b1874c5b5abed38a9012715` |
| patched-24 | `patched-24` | `20ceccdcb507f80d2c41198046e037ce8fa6381f217c5861aad0dafdc4c01744` |
| patched-25 | `patched-25` | `351f1f6e5a711ea4f4ed99a5ab8fe5ce51e7c13d089db38ae452bd85ace3038f` |
| patched-26 | `patched-26` | `d213877c60ec3aad10cb9b16b79f0c38ab95a7cd3f8aa0a7f4f0e1bd433e27b1` |
| patched-27 | `patched-27` | `f384159751fed249263591ff46758bb32327d0048e0669747050b66db1e33c6a` |

说明：patched-25/26/27 的 SHA 为 2026-08-20 release 审阅修复构建可复现性（目录 mtime 归一化）后
重建的值；每个包均以相同 wrapper 独立构建两次并逐字一致。

patched-8 是更早的历史恢复包，当前仓库没有能与该 deb 逐字对应的构建提交，因此不创建会造成
错误追溯的源码 tag；它继续由文件名和 SHA-256 记录。

`patch-001/005/008` 是源码阶段补丁，不是独立的 deb 发布版本：`patch-001` 当前作为
Debian 6.12 兼容基础始终应用，`patch-005` 当前关闭，`patch-008` 仅用于 patched-20
历史诊断且当前关闭，因此不为它们创建与 release 包混淆的同名 Git tag。patched-18/19/20
同样是历史中间结果，且当前源码已禁止按原版本号重建；它们不创建 release tag。可安装、已
完成对应追溯和验收的版本使用上表的 `patched-*` tag。

## 发布最终审阅

发布前必须逐项确认：

1. tag 指向的提交与对应文档、版本号和 SHA-256 一致。
2. `scripts/check-release-package.sh`、`scripts/check-docs.sh`、Shell 语法检查和包边界测试通过。
3. Deepin 原包来源、补丁开关、DKMS 构建、固件/用户态完整性和可复现构建证据齐全。
4. 当前设备运行验证、跨硬件限制、已知问题和默认安装策略写入文档。
5. patched-17 回退包可用，回退命令、SSH/TTY 恢复路径和风险说明经过实际演练；2026-08-17 已完成
   patched-23 -> patched-17 -> patched-23 两次重启验证。
6. release 附件只上传允许的 deb、哈希和说明，不上传历史 patched-20 或 Git 忽略目录中的无关包；
   上传任何二进制前必须通过对应制品门禁与独立权利审查。本地 `debs/`、`vendor/` 内容不参与发布。

构建脚本要求显式设置大于 20 的新版本号，并默认把输出写入本目录，例如：

```sh
PATCH_VERSION=N SOURCE_DATE_EPOCH=<已审查的UTC时间戳> \
  [已审查的补丁开关...] scripts/build-deepin-coherent.sh
```

patched-21 已由 `scripts/build-patched21-deepin-release-candidate.sh` 固定定义；不要手工复制上述
示例并改变其开关。其设计和分阶段验收状态见
[`docs/patches/patched-21-release-candidate.md`](../docs/patches/patched-21-release-candidate.md)。可通过
`OUT_DEB` 指定其他输出路径。构建器会调用 `scripts/check-release-package.sh`；提交前还需确认
`git status --ignored` 中的包仍被忽略，release 上传不应反向修改源码目录。
