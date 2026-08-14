# 当前 TODO

## 文档与维护

- [ ] 为每次新候选包建立独立的 `docs/patches/` 说明和 `docs/incidents/` 验收记录。
- [ ] 将长期维护所需的脚本参数逐步收敛为可审查的配置，保持 `scripts/<name>` 兼容入口不变。
- [ ] release 发布前确认 `debs/` 中包与当前状态文档的版本、哈希和验证证据一致。

## 当前活动项

patched-21 已在 [`../patches/patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)
固定输入、补丁集、辅助载荷和验证门槛。本机当前批次已完成：

- [x] 静态检查与 7 项包边界 fixture；
- [x] 从 Deepin 202504 原 deb 构建 p21，并以固定 epoch 重复构建确认逐字一致；
- [x] 记录 control 字段、文件清单审计和 SHA-256；
- [x] 在 p17 回退、DKMS、headers 和磁盘空间均已确认后部署 p21；完成受控重启；
- [x] 完成 p21 的 PVR、DRM/fbdev、Xorg/GLX、真实 VT fbterm、xdisplay、Picom、音频和桌面验收。

后续只保留发布前工作：扩展坞、三块及以上外屏、无盖桌面和其他硬件的实机矩阵，p17 回退演练，以及
release 审阅。任一验证失败先进入恢复路径和事故记录，不做模块热切换。

显示引擎代码、配置和内部测试已收敛回 dotconfig 维护。本项目当前只保留 Innogpu 设备钩子、会话
接入和安装边界测试；后续 xdisplay 功能不再在本仓库重复实现。跨项目实机矩阵记录在
`suspended.md`。

Picom patch、配置和安装流程已按 `picom-integration.md` 完成吸纳。升级上游 Picom 时需要重新
审查固定基线 patch。
