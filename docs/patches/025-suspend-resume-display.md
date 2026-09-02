# patch-025-suspend-resume-display：resume 重复光标恢复隔离

## 状态

`4.0.1-i2` 的待验证候选修复。已完成静态分析、离线 DKMS 构建和包边界检查；
**未安装、未执行挂起、不得声称红屏已修复**。当前运行版仍为 `4.0.0-i1`，
`4.0.1-i1` 仍是仅供复现的失败候选。

## R03 证据与根因假设

R03 保存的 `build/r03-b1-kernel.log` 在 11:55:10--11:55:31 明确记录了真实
s2idle entry/exit、PVR `1 -> 0 -> 1` 电源恢复和 MCU/DDR 重新初始化。该窗口没有
`3900372`、`PVRSRVEPowerLock failed` 或 PVR 错误计数增长，但外屏实际呈现整屏红色。

可审计的恢复路径存在一次重复硬件编程：

1. `innodpu_drm_wakeup()` 调用 Linux `drm_atomic_helper_resume()`，回放挂起前的 CRTC、
   connector 和 plane atomic state。
2. 本驱动的 `pdp0_crtc_atomic_enable_legacy()` 已在 atomic commit 内部检查
   `cursor_enable`/`cursor_is_disable` 并调用 `cursor_resume()`。
3. atomic resume 成功返回后，`innodpu_drm_resume()` 又遍历所有 CRTC 调用
   `innodpu_pdp0_wakeup()`。该符号在公开 C 源码中只有声明，实现来自
   `vendor/kernel/innosrvkm/innosrvkm.o_shipped`。
4. 对该对象的只读 `readelf`/`objdump` 检查显示：`innodpu_pdp0_backup()` 是空函数；
   `innodpu_pdp0_wakeup()` 在 `cursor_enable` 为真时直接再调用一次
   `pdp0_cursor_resume()`。后者写入光标地址、尺寸、位置和 config-valid 寄存器。

因此最可能的根因假设是：**atomic state 已完整恢复光标后，厂商 post-atomic
hook 又使用保留的 hwdev 光标字段重复编程扫描输出，破坏了刚提交的显示状态**。
这是由调用链和黑盒符号反汇编支持的候选根因，但整屏红色的直接寄存证据尚未取得，
必须通过后续真机 A/B 才能确认。

## 否定性证据与边界

- 不是已观测到的 PowerLock 故障：R03 s2idle 窗口中 PVR 电源顺序完整，且目标
  错误行未出现。但缺少同方法的 `4.0.0-i1` s2idle 对照，不把“错误消失”单独
  归功于 patch-024。
- 不支持 patch-024 直接导致红屏：patch-024 只修改 `pvr_dvfs_device.c` 的 OFF 状态
  devfreq 早退，不改 DRM/KMS、plane、cursor、HDMI 或 GEM 实现。
- 日志中只有 EDID polling 和 PVR 电源证据，没有启用 DPU atomic/cursor 调试输出；
  Xorg/GL 探针成功也不能证明实际扫描像素正确。
- 公开搜索未找到 Innosilicon FH2M 或同一黑盒 hook 的可直接移植红屏修复；
  下述上游材料只证明 atomic helper 已负责完整 state replay。

## 修复设计

`patches/025-suspend-resume-display.patch` 只修改 `innosrvkm/innodpu_drm_pm.c`：保留
GEM recover、`drm_atomic_helper_resume()`、fbdev/polling 恢复和所有 DPU/HDMI 子设备 PM
回调，仅删除 atomic resume 之后的 `innodpu_pdp0_wakeup()` CRTC 遍历。

patch-024 保持不变。这个修复不修改 ABI、GEM 内容、framebuffer 基址、HDMI 链路或
atomic commit 顺序，只保证光标恢复由 atomic CRTC enable 执行一次。

## 版本、构建与验收

- 新候选：`4.0.1-i2`；不复用失败候选 `4.0.1-i1`。
- 固定 epoch：`1788364800`（`2026-09-03 00:00 +0800`），取 R04 后一个本地自然日边界；
  其他版本/epoch 失败关闭。
- `scripts/build-innogpu-driver.sh` 保留显式 `4.0.1-i1` 复现路径（只应用 patch-024）；
  默认 `4.0.1-i2` 在编译 staging 和包内 DKMS 源码中均按序应用 patch-024 +
  patch-025-suspend-resume-display。
- 静态门禁只能证明补丁可应用、范围、版本/epoch 和双树接线正确。真机验收
  须另开轮次，保持挂起冻结、独立回退包、watchdog 和人工在场要求。

## 下一轮观测方案

安装前对照记录 connector/CRTC/plane/framebuffer/cursor 状态；候选启动后先做非挂起显示
基线。挂起测试时在内核日志中开启 DPU atomic/cursor 路径的受控输出，并在 resume
前后采集 DRM debugfs state、CRTC/plane FB ID、cursor enable/尺寸/位置与相关寄存器。成功
判据必须同时包含人工可见画面、SSH、TTY、无 PowerLock/PVR 新错误；任一失败即
回退并保存上一启动 journal。

## 参考

- [Linux v6.12 `drm_atomic_helper_resume()`](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_atomic_helper.c#L3585-L3618)：
  reset software state 后提交 suspend 时保存的完整 duplicated atomic state。
- [Linux v6.12 Rockchip DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/rockchip/rockchip_drm_drv.c#L241-L260)：
  master resume 交给 mode-config helper，没有在 state replay 后再逐 CRTC 恢复 cursor。
- [Linux v6.12 Tegra DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/tegra/drm.c#L1340-L1357)：
  同样由 mode-config helper 完成 master suspend/resume。

上述为恢复责任边界的社区对照，不是 FH2M 红屏的直接同型修复。
