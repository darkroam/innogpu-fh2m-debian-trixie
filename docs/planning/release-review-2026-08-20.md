# Release 审阅记录（2026-08-20）

审阅对象：patched-25 / patched-26 / patched-27 三连候选包发布前综合检查。
审阅分支：docs/release-review；发现并修复 **1 个 release 阻断问题**（构建可复现性）。

## 结论

- 三个候选包的**功能验收均已通过**（patch-025/026/027 实机验证见各自文档）。
- 审阅发现 **deb 构建不可复现**（同一 wrapper 两次构建字节不同），已修复构建器并全部重建；
  修复后每包两次独立构建逐字一致。**修复前的 SHA 作废**，以本记录值为准。
- 修复仅影响 tar 元数据（目录 mtime），**无功能变化**；设备已安装的 patched-27 无需重装，
  release 附件使用重建后的可复现 deb。

## 阻断问题与修复

### 问题：目录 mtime 未归一化导致 deb 不可复现

- 现象：build-deepin-coherent.sh 同一 wrapper 两次构建 patched-27，SHA-256 不同。
- 定位：解包对比两个 deb，control、ar 成员、解包文件内容全部一致；差异在
  data.tar.xz 内**目录条目的 mtime**（分别为两次构建的实时钟 15:54 / 16:04）。
  dpkg-deb --build 保留磁盘上目录的实际 mtime，未应用 SOURCE_DATE_EPOCH；
  构建期新建的目录（install -d 等）因此带上构建时刻。
- 修复：构建器在 dpkg-deb --build 前对整个打包树执行 find + touch 命令，
  把所有条目（文件 + 目录 + 符号链接）的 mtime 归一化到已审查 epoch。
- 验证：patched-25/26/27 各自用同一 wrapper 独立构建两次，SHA-256 逐字一致：

| 包 | 可复现 SHA-256 |
| --- | --- |
| patched-25 | 351f1f6e5a711ea4f4ed99a5ab8fe5ce51e7c13d089db38ae452bd85ace3038f |
| patched-26 | d213877c60ec3aad10cb9b16b79f0c38ab95a7cd3f8aa0a7f4f0e1bd433e27b1 |
| patched-27 | f384159751fed249263591ff46758bb32327d0048e0669747050b66db1e33c6a |

## 逐项核对（debs/README.md 发布最终审阅清单）

| # | 检查项 | 结果 |
| --- | --- | --- |
| 1 | tag 指向提交与文档/版本/SHA 一致 | 通过：25/26/27 tag 指向各自合并提交；SHA 已更新为可复现值 |
| 2 | check-release-package / check-docs / shell 语法 / 包边界测试 | 通过：三包 PASS_RELEASE_PACKAGE_BOUNDARIES；check-docs PASS；全部脚本 bash -n 通过；7 项包边界 fixture 通过 |
| 3 | 原包来源、补丁开关、DKMS 构建、固件/用户态完整性、可复现证据 | 通过：Deepin 202504 原包基线；wrapper 固定开关与 epoch；check-deb-dkms-build 三包 PASS（vermagic 匹配）；可复现性本次修复并验证 |
| 4 | 当前设备运行验证、跨硬件限制、已知问题、默认安装策略写入文档 | 通过：当前运行 patched-27（README/status/architecture 已同步）；跨硬件矩阵未完成（suspended.md）；已知问题在 status.md |
| 5 | patched-17 回退包可用、回退路径演练 | 通过：回退包在 debs/；2026-08-17 已完成 p23 到 p17 到 p23 演练；当前链 patched-27 到 patched-17 到 patched-8 |
| 6 | release 附件只上传允许的 deb/哈希/说明 | 通过：上传清单 = 三个可复现 deb + debs/README.md；不含 patched-20 或忽略目录内容 |

## 变更清单（本次审阅）

1. scripts/build-deepin-coherent.sh：构建前归一化树 mtime（可复现性修复）。
2. debs/README.md：更新 25/26/27 SHA 为可复现值，加说明。
3. docs/project/architecture.md：当前运行驱动 p24 改为 p27，补 p25-27 描述。
4. docs/project/status.md：当前运行驱动行更新为 patched-27。
5. README.md：当前状态更新。
6. scripts/check-docs.sh：architecture require_text 同步为 patched-27。
7. 新增本审阅记录；tag patched-25/26/27 按新 SHA 重建。

## 遗留事项（不阻断本次发布）

- 跨硬件实机矩阵（扩展坞/多屏/无盖桌面/其他机型）仍未完成，见 suspended.md。
- 电源/合盖矩阵仍待完成。
- patched-25/26/27 为增量正确性修复；后续优化候选（invisible READ 预取等）不受影响。
