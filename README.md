# innogpu-fh2m-debian-trixie

Debian Trixie 上 Innosilicon Fantasy II-M（FH2M）驱动、显示输出、硬件 GL、音频和验收项目。
本页只保留当前结论、关键历史边界和文档入口；详细过程不在根 README 展开。

## 当前状态

最后更新：2026-08-18。

- 当前设备运行 `3.3.3.42-patched-24`，内核为 `6.12.101+deb13-amd64`。
- patched-24 已重启验证：DKMS、Driver/Firmware、`/dev/dri/card0`、`renderD128`、`/dev/fb0`
  和 boot autoload 正常。
- p24 是 p23 行为基线加 `6.12.101+` 的 `pci_resize_resource()` 兼容修复；包、校验值和 tag
  见 [p24 验收记录](docs/patches/patched-24-kernel-612101.md) 和 [release 目录说明](debs/README.md)。
- 当前设备此前已完成 p21 的完整 Xorg/GLX、DRI3、真实 VT、显示和 Picom 验收；p24 本次检查未把
  隔离会话中的 Xorg/dwm 进程读取结果计入新增证据。

## 关键边界

- 后续包只能以 Deepin `20250421190503-debug` 完整原包为技术基线，不能从历史 patched 包拼接用户态文件。
- `patched-17` 是保守回退点，`patched-8` 是更早历史恢复物；p20 及更早中间版本只保留历史证据，禁止新设备部署。
- 当前回退链：`patched-24 -> patched-23 -> patched-22 -> patched-21 -> patched-17 -> patched-8`。
- xdisplay 引擎由 dotconfig 仓库独立维护；本项目只维护 Innogpu 设备接入契约。
- `.deb`、Deepin 原包和本机日志不进入 Git；release 包放在 `debs/`，由独立 release 附件分发。

## 文档入口

| 目标 | 文档 |
| --- | --- |
| 当前状态、风险和未完成事项 | [docs/project/status.md](docs/project/status.md) |
| 架构、所有权和组件边界 | [docs/project/architecture.md](docs/project/architecture.md) |
| 新设备安装 | [docs/user/new-device-install.md](docs/user/new-device-install.md) |
| 安装后验证 | [docs/user/verification.md](docs/user/verification.md) |
| 黑屏、TTY、Xorg 或驱动恢复 | [docs/user/recovery.md](docs/user/recovery.md) |
| 补丁和版本验收 | [docs/patches/README.md](docs/patches/README.md) |
| 依赖、外部包和目录结构 | [docs/project/dependencies.md](docs/project/dependencies.md) |
| 失败过程和根因 | [docs/incidents/README.md](docs/incidents/README.md) |
| 实施历史和待办 | [docs/planning/history.md](docs/planning/history.md)、[todo.md](docs/planning/todo.md) |
| 脚本使用和风险 | [scripts/README.md](scripts/README.md) |

完整文档索引见 [docs/README.md](docs/README.md)。

## 最小安全原则

1. 安装或升级前先确认 release 包、SHA-256、回退包和 SSH/真实 TTY 恢复路径。
2. 先更新文档，再修改代码；修改后运行对应测试并复核当前状态文档。
3. 安装新驱动后先验证包、DKMS 和节点，再决定是否重启；不要热切换活动显卡模块。
4. 任何黑屏或 Xorg 失败，按 [故障恢复](docs/user/recovery.md) 回退，不复制历史包中的单个 `.so`、固件或 `.ko`。

## 仓库结构

```text
patches/       内核、Picom 和 fbterm 补丁
scripts/       构建、安装、恢复和验证入口
tests/ tools/  自动化测试与最小探针
config/        本项目维护的配置模板
docs/project/  当前架构、状态、依赖和维护规则
docs/patches/  补丁与 release 验收记录
docs/user/     安装、验证、显示和恢复操作
docs/incidents/失败现场与根因
docs/planning/ 计划、实施历史和待办
debs/          被 Git 忽略的本地包目录，仅跟踪说明
```
