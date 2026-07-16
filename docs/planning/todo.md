# 当前 TODO

## 显示代码吸纳

- [ ] 将 dotfiles 提交 `5628c6e` 中的 `xdisplay.sh` 作为来源，吸纳到本项目的通用显示脚本。
- [ ] 吸纳与 watcher 共用 apply lock 的 `displayselect`。
- [ ] 从完整 `xprofile` 中只提取显示启动契约，不引入代理、输入法、Picom 或 MPD 配置。
- [ ] 保留 `restore-dp1-mode-x11.sh` 为 Innogpu 设备钩子，通用引擎不保存固定 modeline。
- [ ] 更新 `prepare-soft-xorg-dwm.sh`，禁止它再次安装旧硬编码显示脚本。
- [ ] 更新 deb 构建与卸载清单，使包内辅助工具和仓库脚本一致。
- [ ] 选择性迁移静态 RandR fixture 和测试 runner，排除运行日志、锁和隔离备份。
- [ ] 运行静态、fixture、当前 X11 和实机显示矩阵。
- [ ] 验证后更新 `project/display-management.md` 和 `history.md` 的状态。

## 文档维护

- [ ] 增加自动文档一致性检查脚本。
- [ ] 检查所有旧链接并保留必要的兼容指引。
- [ ] 在代码吸纳完成后复核 README、安装、验证和恢复文档。
