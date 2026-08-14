# 事故与经验

本目录保存已经完成定位或形成明确边界的失败现场。每份记录按“现象 → 证据 → 根因/排除项 →
修复或边界 → 后续门槛”组织，避免把历史失败误读为当前状态。

## 记录索引

| 记录 | 结论 |
| --- | --- |
| [patched-17 fbdev mmap](patched-17-fbdev-mmap.md) | `/dev/fb0 mmap()` 的 `ENODEV` 会使 fbterm 在绘制阶段崩溃；`fb_io_mmap` 修复已验证 |
| [patched-18 用户态混配](patched-18-userspace-mix.md) | 新 DKMS 源码与旧包用户态不能拼接，必须保留同一 Deepin 发布的完整 ABI 载荷 |
| [patched-18 shader 固件](patched-18-shader-firmware.md) | shader 固件缺失使 RGX 进入 BAD，services open 后续返回 `ENODEV` |
| [patched-20 运行时验收](patched-20-runtime.md) | 完整 Deepin 202504 载荷越过 PVR、隔离 Xorg/GLX 和真实 VT 门槛；诊断补丁仍非长期方案 |
| [patched-20 旧辅助载荷](patched-20-legacy-helper-payload.md) | 运行验收 deb 早于 xdisplay 所有权收敛，版本不可复用，包不可直接发布 |
| [Picom GL 能力判断](picom-explicit-uniform-location.md) | 下游 Picom 用最小 shader 验证能力，不修改预编译驱动库或伪造扩展字符串 |

## 使用规则

- 事故记录是过程和边界证据，不是当前安装状态；当前结论以 [`project/status.md`](../project/status.md) 为准。
- 原始内核、Xorg、GLX、EDID 和 strace 文件保留在本机，不提交到 Git；文档只保存必要的摘要和可复现命令。
- 新问题应先增加记录，再修改补丁或安装流程，并在记录末尾写明验证结果和回退条件。
