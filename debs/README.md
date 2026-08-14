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

## 输出包

构建脚本默认把输出写入本目录，例如：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-20.deb
```

可通过 `OUT_DEB` 指定其他输出路径。提交前确认 `git status --ignored` 中的包仍被忽略，release
上传不应反向修改源码目录。
