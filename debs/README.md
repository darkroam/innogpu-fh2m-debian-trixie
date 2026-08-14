# Release 包目录

`debs/` 是本地 release 和构建输入目录。目录中的 `.deb` 文件不进入 Git；发布时由维护者
从这里挑选并上传到 release。仓库只跟踪本说明文件，避免把大体积二进制包和本机版本状态混入源码历史。

## 输入包

后续 coherent 构建以 Deepin 202504 原包为唯一技术基线：

```text
debs/innogpu-fh2m_20250421190503-debug_amd64.deb
```

`scripts/build-deepin-coherent.sh` 和 `scripts/prepare-deepin-userspace-root.sh` 会优先查找该路径，
并保留仓库根目录的旧路径作为兼容回退。历史 patched 包只用于安装回退或复现记录，不能作为新包输入。

本机保存的 patched-19/20 deb 生成于 xdisplay 所有权收敛之前，包内仍有旧显示引擎和实验辅助文件。
它们只能作为历史/当前机器证据，不得上传为当前 release。当前源码禁止复用 19/20 版本号。

当前本地 p21 输出已通过包边界和两次逐字一致构建，并已在当前设备完成部署、重启和运行验收；实际身份
记录在 [`patched-21-release-candidate.md`](../docs/patches/patched-21-release-candidate.md)。这不等于它
已完成跨硬件 release：维护者不得仅凭文件位于 `debs/` 或当前设备通过就上传 release。

## 输出包

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
