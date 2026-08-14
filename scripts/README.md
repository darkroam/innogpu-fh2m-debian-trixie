# 脚本入口与职责

`scripts/` 保留历史文件名作为稳定 API。安装器、测试、打包流程和外部文档直接调用这些路径，
因此本轮不物理移动或批量改名；新增逻辑应优先抽成小函数或独立脚本，并在这里登记职责。

## 构建与打包

| 入口 | 职责 |
| --- | --- |
| `build-deepin-coherent.sh` | 从 `debs/` 中的 Deepin 202504 原包应用补丁并构建 coherent deb |
| `build-patched19-deepin-coherent.sh` | 历史 patched-19 的固定参数包装入口 |
| `build-patched20-deepin-diagnostic.sh` | patched-20 诊断候选的固定参数包装入口 |
| `prepare-deepin-userspace-root.sh` | 解包 Deepin 用户态到忽略的 `third_party/` 目录 |
| `build-patched17-deepin-local-display.sh`、`build-patched18-deepin-local-display.sh` | 已停用的历史构建护栏，明确拒绝重建 |

## 安装、回退与系统集成

`install*.sh` 负责依赖、驱动包、用户态、音频和 X11 会话安装；`uninstall*.sh` 与
`disable-incompatible-userspace.sh` 负责回退边界。`install-patched17-and-check.sh` 是当前保留的
patched-17 回退入口，patched-8 仅用于更早历史恢复。

## 诊断与验证

`check-*.sh`、`test-*.sh`、`run-*.sh` 和 `verify-install-status.sh` 只读检查或创建临时测试环境；
改变系统状态的脚本必须在文件头和用户文档中明确标注 root、重启、modeset 或卸载风险。测试生成的
原始日志留在本机临时目录或 `baselines/` 的忽略路径，Git 只保留精简结果。

## 显示与 Picom

`xdisplay.sh` 是 watcher 主实现，`displayselect` 是手动布局入口，`xdisplay-session.sh` 和
`install-xdisplay-user.sh` 管理会话接入；`picom-session.sh`、`build-patched-picom.sh` 和
`install-picom-user.sh` 管理独立 Picom 用户态流程。显示管理的详细职责见
[`docs/project/display-management.md`](../docs/project/display-management.md)。

## 修改规则

代码路径是兼容接口：改名或移动前必须扫描 `scripts/`、`config/`、`tests/`、桌面源码和服务入口，
提供包装器并同步文档。完整的文档优先、验证、隐私和 release 约束见
[`docs/project/maintenance-policy.md`](../docs/project/maintenance-policy.md)。
