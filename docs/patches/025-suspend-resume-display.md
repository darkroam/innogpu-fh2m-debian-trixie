# patch-025-suspend-resume-display：resume 重复光标恢复隔离

## 状态

`4.0.1-i2` 已在 R05 完成一次 s2idle 可见恢复验收，但单次成功不足以证明根因。R06 以同
epoch 的 `4.0.1-i3`（A：仅 patch-024）与 `4.0.1-i4`（B：024+025）建立严格单变量对照；
A 的第 1/4 轮正常恢复，但三个 post-atomic hook 均未进入 cursor restore，按预定停止条件
终止后续盲测。当前运行 i3；patch-025 仍为 **UNVERIFIED**，不得声称根因已证实或已修复。

## R03 证据与候选机制

R03 保存的 `build/r03-b1-kernel.log` 在 11:55:10--11:55:31 明确记录真实 s2idle
entry/exit、PVR `1 -> 0 -> 1` 电源恢复和 MCU/DDR 重新初始化。该窗口没有 `3900372`、
`PVRSRVEPowerLock failed` 或 PVR 错误计数增长，但外屏实际呈现整屏红色。

可审计的恢复路径存在一次条件式重复硬件编程：

1. `innodpu_drm_wakeup()` 调用 Linux `drm_atomic_helper_resume()`，回放挂起前的 CRTC、
   connector 和 plane atomic state。
2. 本驱动的 `pdp0_crtc_atomic_enable_legacy()` 已在 atomic commit 内部检查
   `cursor_enable`/`cursor_is_disable` 并调用 `cursor_resume()`。
3. atomic resume 返回后，`innodpu_drm_resume()` 又遍历所有 CRTC 调用
   `innodpu_pdp0_wakeup()`。该符号实现来自
   `vendor/kernel/innosrvkm/innosrvkm.o_shipped`。
4. 对该对象的只读 DWARF 与反汇编检查显示：`innodpu_pdp0_backup()` 是空函数；
   `innodpu_pdp0_wakeup()` 在 `cursor_enable` 为真时直接再调用 `pdp0_cursor_resume()`。
   后者把 cursor framebuffer 低位写入 HAL register ID `0x259`，把地址高位与宽高编码
   写入 `0x25a`，把 x/y 与 `0x0c000000` 控制位写入 `0x258`，随后调用
   `set_config_valid(hwdev, 1)` 提交影子配置。`0x258..0x25a` 是 HAL ID；现有材料不能
   把它们可靠换算为物理 MMIO 地址。

因此一个有静态机制支持的条件假设是：**仅当某个 DPU 的 `cursor_enable` 保留为真时，
atomic state 恢复后厂商 post-atomic hook 又使用陈旧 hwdev 光标字段重复编程，并通过
config-valid 提交错误的 DPU 影子状态**。R06/R07 均未使该分支进入实验组；整屏红色的
直接寄存器证据也未取得，所以它只是候选根因，不是当前“最可能且已验证”的结论。

## 否定性证据与边界

- 不是已观测到的 PowerLock 故障：R03 s2idle 窗口中 PVR 电源顺序完整，且目标错误行
  未出现。但缺少同方法的 `4.0.0-i1` s2idle 对照，不把“错误消失”单独归功于 patch-024。
- 不支持 patch-024 直接导致红屏：patch-024 只修改 `pvr_dvfs_device.c` 的 OFF 状态
  devfreq 早退，不改 DRM/KMS、plane、cursor、HDMI 或 GEM 实现。
- R03 日志中只有 EDID polling 和 PVR 电源证据，没有 DPU atomic/cursor 字段快照；
  Xorg/GL 探针成功也不能证明实际扫描像素正确。
- 公开搜索未找到 Innosilicon FH2M 或同一黑盒 hook 的可直接移植红屏修复；下述上游材料
  只证明 atomic helper 已负责完整 state replay。

## 候选补丁与构建边界

`patches/025-suspend-resume-display.patch` 是带三行上下文的纯删除补丁，只修改
`innosrvkm/innodpu_drm_pm.c`：保留 GEM recover、`drm_atomic_helper_resume()`、
fbdev/polling 恢复和所有 DPU/HDMI 子设备 PM 回调，仅删除 atomic resume 之后的
`innodpu_pdp0_wakeup()` CRTC 遍历。patch-024 保持不变。

这个候选不修改 ABI、GEM 内容、framebuffer 基址、HDMI 链路或 atomic commit 顺序；
它删除已知的第二个恢复入口，但尚未证明该入口在红屏事故中执行，也未证明真实掉电后的
deep 路径不再需要它。

- 严格对照：A=`4.0.1-i3` 只应用 patch-024；B=`4.0.1-i4` 在其后应用 patch-025；
  i1/i2 不再由当前构建器复用。
- i3/i4 共用固定 epoch `1788451200`（`2026-09-04 00:00 +0800`）；其他版本/epoch
  失败关闭。
- 构建器以 `--fuzz=0 --no-backup-if-mismatch` 应用补丁；任一 hunk 失败即终止，并在编译
  staging、包内 DKMS 源码与最终 package payload 三处拒绝任何 `.orig/.rej`。
- 静态门禁只能证明补丁可应用、范围、版本/epoch 和双树接线正确。真机验收须另开轮次，
  保持独立回退包、watchdog 和人工在场要求；deep 继续冻结。

## R06 单变量与执行证据

旧 i2 包含未参与编译但不应入包的
`usr/src/innogpu-kernel-2.2/innosrvkm/innodpu_drm_pm.c.orig`；它由零上下文 025 的
offset 应用触发 GNU patch mismatch backup。R06 在首轮安装前停止并修正：025 重生为
三行上下文补丁，release package gate 与 fixture 同时拒绝 `.orig/.rej`。i3 SHA-256 为
`6cab9e521046b386ec2e34ce84d384302b044a4c215c836566274d5de769dcba`，i4 为
`085e06844607a9973a6d5e3c1e3c4ec986a1cdf6903e6a9169b68285e39969a7`。完整解包比较显示
两包路径、类型、权限和符号链接目标一致；排除目标源码后 payload 内容零差异，目标源码
只差 025 的 6 行删除；`DEBIAN/` 只差版本、描述与版本提示；两包均无 `.orig/.rej`。

R06 随后在 i3 上完成第 1/4 轮 s2idle：固定 HDMI-2/1920x1080，人工画面、SSH、TTY 和
PVR 门禁均正常。ftrace 计数为 `drm_atomic_helper_resume=1`、
`pdp0_crtc_atomic_enable=1`、`innodpu_pdp0_wakeup=3`、`pdp0_cursor_resume=0`。A 未稳定
复现红屏，且待比较的 cursor 处理没有执行；继续安装 i4 只能比较“删除未执行分支”，
不满足有效 A/B。按预先批准的停止条件终止 i4->i3->i4，不做 deep，机器保留 i3。

该结果削弱“只要存在 post-atomic hook 就必然红屏”的强假设，但不排除仅在
`cursor_enable=1` 或特定 DPU 状态下触发的条件假设。R03 红屏窗口没有同组 ftrace/字段
快照，不能反推当时 cursor 分支为真。

## R07 非挂起观测

R07 新增失败关闭的只读 bpftrace 观测器，对固定内核、i3 包、厂商对象 hash 和 DWARF ABI
偏移做门禁，只记录自然发生的 cursor/config-valid 调用和 HAL 访问。2026-09-02 在当前
HDMI-2 主输出上持续移动鼠标的 5 秒样本得到：

- 50 个 `pdp0_set_config_valid` 状态事件，全部来自 dpu1；DRM debugfs 同时确认 dpu1
  `active=1`、primary plane FB 83、1920x1080，HDMI-2 connected/enabled/DPMS On；
- 事件中的 `hwdev->crtc` 有效，但该厂商路径保存的 `crtc->state` 指针为空，因此 BPF
  `active` 字段明确标为 unavailable；活动状态以同时间窗只读 DRM debugfs 为准；
- 所有事件 `cursor_enable=0`，`pdp0_cursor_move/set/resume` 均未出现，`0x258..0x25a`
  自然读写均为 0。primary scanout 内容 CRC 没有已确认的安全只读内核接口，明确记为
  unavailable，不用截图或 fbdev hash 冒充。

当前 Xorg 明确使用 `modesetting` DDX；内核 `inno_dpu_cursor_set()` 是空实现，普通指针移动
没有进入厂商 `cursor_set2/move`。这与 R06 `cursor_resume=0` 一致，证明当前日常桌面不满足
cursor 假设的入组条件，但仍不能说明 R03 红屏时的状态。

## 次优假设与原则性修复

现有证据按优先级保留四条假设：

1. 条件式 cursor/config-valid 错序：仅在 `cursor_enable=1` 时重复或错误 DPU 提交破坏输出；
   静态调用链支持，但运行时尚未入组。
2. primary framebuffer/GEM 恢复或 cache/coherency 竞态：整屏单色比局部 cursor 损坏更符合
   scanout 内容或地址异常；R03 缺少 FB 地址、内容 CRC 与 cache 状态证据。
3. HDMI mode/color/link 恢复竞态：R03 和正常轮次都有 EDID polling，说明它不是必现；仍缺
   红屏瞬间 output format、link 与 PHY 读回。
4. 与 cursor 无关的 DPU shadow/config-valid 时序：post hook 遍历包含 inactive CRTC；若共享
   shadow bank 被陈旧实例提交，可能影响活动 dpu1，当前无 bank 读回。

`innodpu_pdp0_backup()` 为空，现有实现没有 suspend 快照代次或“本轮已由 atomic 恢复”标志。
不能按 `s2idle`/`deep` 名称决定是否恢复 cursor；R06 s2idle 同样发生 PVR 掉电恢复，真实
DPU reset/寄存器 retention 才是条件。patch-025 的纯删除可能在某些 deep 路径漏掉 legacy
cursor 恢复，所以 deep 验证前不得晋级。

原则性修复应在 suspend 时保存每个 hwdev 的 cursor 状态与恢复 generation；确认 DPU reset
后，只由 atomic CRTC enable 在 modeset 完成后恢复一次并清除 pending，post hook 仅处理
atomic 未覆盖且 pending 的实例。长期应把 legacy cursor 纳入 DRM plane atomic state，删除
并行的恢复状态机，而不是无条件重复或无条件删除。

## 后续受控实验设计（本轮不执行）

1. 先在独立轮次、用户明确同意和可完整回退条件下验证硬件光标入组。当前使用 modesetting
   DDX，单独添加 `SWCursor=false` 不足以证明会调用厂商 `cursor_set2`；应先临时试验 vendor
   DDX 或经 Xorg DRM master 发起 cursor2 的可行路径，重启 X 前保存配置与回退命令。只以
   观测到 `pdp0_cursor_set/move`、`cursor_enable=1` 和目标寄存器自然写入作为入组成功。
2. 入组后才规划 i3 最小一次 s2idle：用户在场，先复核回退包，布设 RTC 与 300 秒 watchdog，
   同时采集本工具、resume 四函数 trace、DRM state、journal 与人工画面。若 A 不稳定复现红屏，
   立即停止，不以重复次数代替触发控制；只有 A 可重复失败才恢复 i3/i4 A/B。
3. 若硬件光标始终不能入组，停止 patch-025 因果试验，转向假设 2/4：观测 primary plane FB ID、
   `pdp0_get_fb_dev_paddr` 自然返回、可用时的 DRM CRTC CRC，以及 primary/shadow/config-valid
   相关 HAL 写入序列。没有安全 CRC 或自然 readback 时记 unavailable，禁止主动调用 HAL、
   直接读取未知 MMIO 或把 X 截图当 scanout CRC。

## 参考

- [Linux v6.12 `drm_atomic_helper_resume()`](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_atomic_helper.c#L3585-L3618)：
  reset software state 后提交 suspend 时保存的完整 duplicated atomic state。
- [Linux v6.12 Rockchip DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/rockchip/rockchip_drm_drv.c#L241-L260)：
  master resume 交给 mode-config helper，没有在 state replay 后再逐 CRTC 恢复 cursor。
- [Linux v6.12 Tegra DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/tegra/drm.c#L1340-L1357)：
  同样由 mode-config helper 完成 master suspend/resume。

上述为恢复责任边界的社区对照，不是 FH2M 红屏的直接同型修复。
