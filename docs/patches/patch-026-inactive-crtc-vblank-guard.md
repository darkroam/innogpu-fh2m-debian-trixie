# patch-026：未活动 CRTC 的 vblank 守卫

## 目的

修复 `pdp0_crtc_enable_vblank()` 对未活动 CRTC 无条件置位 `vblank_enable` 并返回成功的问题。
这是正确性/健壮性修复：让错误用户态对未活动 CRTC 的 vblank 请求**立即失败**，而不是永久阻塞在
`WAIT_VBLANK`。

## 背景

- 位置：`innosrvkm/pdp0_crtc.c` `pdp0_crtc_enable_vblank()`（第 1114 行）。
- 现状：真实显示路径（非 nulldisp）直接 `atomic_set(vblank_enable, 1)` 并返回 0，不检查
  CRTC 是否活动、是否有有效 mode。
- 实测（见 [webkit-dmabuf 调查](../planning/webkit-dmabuf-investigation.md)）：本机 CRTC 1
  的 vblank 正常（约 15.7–17.7ms 周期），CRTC 0/2 在 300ms 内不返回、CRTC 3 返回 `EINVAL`。
  若用户态（如 GTK 的 vblank monitor 匹配错误）请求未活动 CRTC，会成功创建 monitor 后
  永久阻塞在第一次 relative wait。
- DRM 契约：`drm_crtc_funcs->enable_vblank` 在"该 CRTC 无法提供 vblank 中断"时应返回
  非零错误，内核 `drm_vblank_get` 会把错误返回给用户态。

## 补丁内容

`innosrvkm/pdp0_crtc.c` 真实显示分支：置位前检查 `crtc->state` 存在、`active` 为真、
`mode.clock` 非零；不满足则记录日志并返回 `-EINVAL`。nulldisp（hrtimer 软件 vblank）分支
保持原行为，不检查。

只改这一个函数；不混入 vblank 时序、IRQ 或用户态改动。

## 构建开关

`APPLY_INACTIVE_CRTC_VBLANK_GUARD=1`；候选包 `3.3.3.42-patched-26`，构建入口
`scripts/build-patched26-vblank-guard.sh`（固定 SOURCE_DATE_EPOCH）。

## 验证与回退

- 离线验证（已完成）：完整 p25 补丁集上叠加本补丁，对 `6.12.101+deb13-amd64` 编译成功且
  vermagic 匹配；不安装、不热切换。
- 候选包（已构建）：`debs/innogpu-fh2m-trixie_3.3.3.42-patched-26.deb`，
  SHA-256 `51ddd8cbb024c5893f1d3d0cbdc6bc8f50490a8f0e8d4a9510a9bc3f0d92e14c`；
  `check-release-package.sh` 与 `check-deb-dkms-build.sh` 均通过。
- 实机验证门槛（操作者在真实会话执行）：
  1. 安装 patched-26 并重启，Driver/Firmware OK、桌面硬件 GL 回归；
  2. `tools/probe-drm-vblank.c` 逐 CRTC 探测：活动 CRTC（本机为 1）仍能连续 wait 且序号递增；
     未活动 CRTC 立即返回 `EINVAL`（不再 300ms 超时）；
  3. 桌面显示、Picom/GLX 行为与 p25 一致。
- 回退：直接回退点 `patched-25`；本补丁不改变 ioctl ABI 或内存布局。

## 参考

- 内核 `include/drm/drm_vblank.h` / `drm_crtc.h`：`enable_vblank` 契约。
- [webkit-dmabuf-investigation.md](../planning/webkit-dmabuf-investigation.md) 静态审计第 2 项
  与"DRM vblank"一节。
