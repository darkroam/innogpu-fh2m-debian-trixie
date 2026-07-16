# 显示管理代码吸纳计划

## 状态

状态：文档基线已建立，代码尚未吸纳。

来源为本机 dotfiles 提交 `5628c6e`。当前工作树中的 `~/.local/bin/xdisplay.sh` 与该提交完全一致，
SHA-256 为：

```text
427d56f78ff11482c59c9e4b95f9fc75a1890ca83b83d81956410d17e6690251
```

## 文件映射

| 来源 | 本项目目标 | 处理 |
| --- | --- | --- |
| `~/.local/bin/xdisplay.sh` | `scripts/xdisplay.sh` | 吸纳通用 watcher，保留来源与校验记录 |
| `~/.local/bin/displayselect` | `scripts/displayselect` | 吸纳手动入口和共享 apply lock |
| `~/.config/x11/xprofile` 显示块 | 项目拥有的会话启动片段/安装逻辑 | 只提取 `XDISPLAY_*` 和 watcher 启动 |
| `~/.local/bin/innogpu-restore-dp1-mode-x11` | `scripts/restore-dp1-mode-x11.sh` | 比较并保留为设备钩子 |
| 隔离目录静态 fixtures/runners | `tests/xdisplay/` | 选择性迁移，清除运行产物和个人路径 |
| `scripts/xdisplay.sh.with-innogpu-restore` | 删除 | 旧硬编码实现，不再作为安装来源 |

完整 `xprofile` 不吸纳，因为其中还包含输入法、代理、壁纸、Picom、MPD 等本项目不拥有的配置。
活动 `/etc/X11`、logind 和 udev 文件只记录关系，不直接复制。

## 实施阶段

1. 完成本项目文档重构并提交文档基线。
2. 导入通用 watcher、displayselect 和净化后的 fixture。
3. 设计幂等会话集成，使 `prepare-soft-xorg-dwm.sh` 安装工具时不覆盖用户其他 X11 配置。
4. 更新构建包辅助文件和卸载清单。
5. 运行静态和 fixture 测试，不接触当前 X11 布局。
6. 比较 `xdisplay.sh --status` 与当前 RandR/lid/DRM 状态。
7. 使用受控 watcher 交接验证单实例、锁和退出行为。
8. 最后执行热插、热拔、合盖、开盖和手动布局实机验证。
9. 按结果更新文档状态并提交代码阶段。

## 验收门槛

- Shell 语法检查全部通过。
- fixture 状态解析、watcher 生命周期和 stage4 回归测试全部通过。
- `--status` 不产生 RandR 写操作。
- `prepare-soft-xorg-dwm.sh` 不再降级覆盖已验证 watcher。
- watcher 与 displayselect 对同一 X server 使用同一 apply lock。
- stale 输出不保留 geometry，framebuffer 收敛到有效输出包围盒。
- 没有 preferred 时使用 RandR 实际提供的目标模式。
- 设备钩子缺失或失败不会阻塞 X11 会话。
- 文档不把尚未完成的 manual marker/适配器描述为当前功能。

## 回退

代码吸纳前记录当前仓库提交和已安装脚本校验。若新脚本失败：

1. 停止新 watcher，不同时运行新旧自动布局链路。
2. 恢复本次修改前的项目提交或重新安装上一版项目辅助脚本。
3. 使用 `displayselect` 或已确认的单屏 RandR 布局保持至少一个可见输出。
4. 黑屏时先切回 TTY/SSH，不继续执行新的 modeset 实验。
5. 驱动包本身异常时使用 `patched-8` 回退；仅显示 watcher 异常时不要重装 DKMS。
