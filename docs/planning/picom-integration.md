# Picom 修改吸纳计划

## 状态

状态：2026-07-17 吸纳和非破坏性验证完成。

## 来源

- 源码目录：`$HOME/src/picom`，只作为本机来源，不写入项目脚本；
- 上游基线：`6d676824c457a933c52e3e92c5a1856466f90545`；
- 源码改动：仅 `src/backend/gl/gl_common.c`；
- 配置来源：dotfiles 提交 `7747c2d` 引入的 `~/.config/x11/picom.conf`；
- 启动来源：dotfiles 提交 `7747c2d` 引入并由后续提交保留的 Picom/xcompmgr 条件块；
- 当前 dotfiles 基线：`5628c6e`。

## 吸纳范围

1. 保存可由干净上游基线应用的独立 patch。
2. 保存去除个人路径的 Picom 配置模板。
3. 提取 Picom 单实例启动和 xcompmgr 回退为独立会话片段。
4. 提供依赖、构建、安装和用户配置安装脚本。
5. 在 `/tmp` 的干净源码 clone 中验证 patch、构建和配置安装器。
6. 只读核对当前运行进程、二进制、配置和日志，不重启 Xorg 或当前 Picom。
7. 按验证结果复核项目、用户和历史文档。

## 验收门槛

- patch 只能应用到固定基线，并能由 `git apply --reverse --check` 识别已应用状态；
- 干净源码应用 patch 后 `ninja` 构建成功；
- 用户安装器幂等，不复制完整 `xprofile`，不启动或停止当前合成器；
- 配置语法可由构建后的 Picom 解析；
- 会话片段在 Picom 不存在时能够选择 xcompmgr；
- 仓库不包含 Picom 源码、build 目录、本机日志或绝对 home 路径。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| patch 应用后的源码与本机修改比较 | 逐字一致 |
| `/tmp` 干净 clone 正向应用与反向识别 | 通过 |
| Picom v13 从零构建 | 46 个编译/链接步骤通过 |
| 项目配置与当前运行配置比较 | 逐字一致 |
| Picom `--diagnostics` 配置/GLX 检查 | `Fantasy II-M`，`Accelerated: 1` |
| 用户安装器 | 3 项通过 |
| Picom/xcompmgr 会话选择 | 3 项通过 |
| 当前 X11/Picom 重启或替换 | 未执行，避免中断已验证会话 |

## 回退

1. 停止 Picom，使用 `xcompmgr` 保持基础 X11 合成。
2. 恢复安装器生成的 Picom 配置和二进制备份。
3. 仅 Picom 失败时不要回退 innogpu DKMS 或重启设备。
4. 需要排查 GLX 时禁用模糊和动画，但不要同时修改驱动、Picom 和窗口管理器。
