# patch-025-suspend-resume-display：resume 重复光标恢复隔离

## 状态

`4.0.1-i2` 已在 R05 完成一次 s2idle 可见恢复验收，但单次成功不足以证明根因。R06 以同
epoch 的 `4.0.1-i3`（A：仅 patch-024）与 `4.0.1-i4`（B：024+025）建立严格单变量对照；
A 的第 1/4 轮正常恢复，但三个 post-atomic hook 均未进入 cursor restore，按预定停止条件
终止后续盲测。R10 后续 deep 失败后已回退 `4.0.0-i1`；patch-025 仍为 **UNVERIFIED**，
不得声称根因已证实或已修复，也不进入 `4.0.2-i1` 或 `4.0.2-i2`。

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
- 事件中的 `hwdev->crtc` 有效；R07 当时按 shipped-object 偏移读得 `crtc->state` 为空并把
  `active` 标为 unavailable，R08 后续证实这是 ABI 偏移误读（见下）；
- 所有事件 `cursor_enable=0`，`pdp0_cursor_move/set/resume` 均未出现，`0x258..0x25a`
  自然读写均为 0。primary scanout 内容 CRC 没有已确认的安全只读内核接口，明确记为
  unavailable，不用截图或 fbdev hash 冒充。

当前 Xorg 明确使用 `modesetting` DDX；内核 `inno_dpu_cursor_set()` 是空实现，普通指针移动
没有进入厂商 `cursor_set2/move`。这与 R06 `cursor_resume=0` 一致，证明当前日常桌面不满足
cursor 假设的入组条件，但仍不能说明 R03 红屏时的状态。

R07 关于 `crtc->state` 为空的解释已被 R08 纠正：R07 使用 shipped object 的 DRM DWARF
偏移 `1176` 读取正在运行的内核对象，但加载模块 BTF 中 `drm_crtc.state` 的实际偏移是
`1480`。因此 R07 的空值是 ABI 偏移误读，不能作为厂商路径未保存 state 的证据；R07 原始
轮次记录保留不改。

## R08 primary FB/GEM 与 shadow 基线

R08 将包装器改为按 `/sys/kernel/btf/innogpu` 校验运行模块 ABI，并只记录自然调用。2026-09-02
的首个 15 秒整包装器样本在新接入的 HDMI-2 `2560x1440` 显示器上通过：dpu1 的已完成调用中，
`pdp0_get_fbaddr()`、`pdp0_get_fb_dev_paddr()` 和 `fh2m_innodpu_gem_get_dev_paddr()` 返回的
设备地址逐链一致；观察到 FB 83/84、3 个设备地址、28 个 plane update、78 个 shadow 序列
事件和 26 个 config-valid HAL 写入。样本没有 cursor 入口、cursor 寄存器访问或 GEM free。

随后由 root 直接运行的 10 秒整包装器样本同样通过。采样时外屏已经断开，活动输出变为内屏
eDP-1 `2560x1600`/dpu2；这不是前一 HDMI 样本的模式漂移。该样本的 129 个
`pdp0_set_config_valid()` 事件均由运行 BTF 校验后的偏移读到 `active_valid=1, active=1`，
直接验证了上述 R07 偏移纠正。HAL 原型确认第二、三个参数分别是 `reg_module` 与
`reg_entity`；日志不再把 `reg_module=15` 错标成 `dpu=15`。debugfs 能提供
`output_format=RGB`、`output_bpc=0` 和 `broadcast_rgb=Automatic`，其中 `output_bpc=0`
不能解释为实际线缆位深。

高频样本中 entry 数可大于已捕获 return 数。包装器分别报告 `entries`、`completed` 和
`unmatched_at_stop`，后者可能包含采样停止边界、kretprobe 未命中或事件输出丢失，不能冒充
仍在执行的内核调用。只有带完整 return 的链用于地址一致性判断。primary scanout 内容 CRC、
未知 shadow bank 的自然 readback 仍无已确认的安全只读接口，保持 unavailable。

R07 使用 `1920x1080`，R08 两次分别使用新外屏 `2560x1440` 和内屏 `2560x1600`；显示器与
活动 connector 不同，所以不能直接比较 mode、pitch、FB 地址或单位时间事件数。R08 只建立
正常桌面的健康事件结构：它削弱“正常运行时持续存在 FB/scanout 地址不一致”的说法，但没有
观测 suspend/resume 窗口，故既未证实也未排除假设 2/4，patch-025 继续为 UNVERIFIED。

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

1. 另开轮次并由用户和 dsh 明确批准后，最多在当前 i3 上执行一次 s2idle 复现；本轮不执行。
2. 挂起前复核 `4.0.0-i1` 回退包及 SHA-256，确认用户在场，布设 60 秒 RTC 唤醒和挂起后
   300 秒 watchdog，并验证两者已生效；deep 继续禁止。
3. 挂起前启动 R08 observer 与 resume 四函数 trace，保存 pre DRM primary FB/GEM、shadow、
   connector、PVR 与 journal 快照；恢复后立即保存同组 post 证据和人工内屏/外屏/SSH/TTY 结果。
4. 若红屏复现，只比较完整 entry/return 链、config-valid 次序与 connector 状态，不把截图当
   scanout CRC；按 watchdog/回退协议停止。若一次未复现或出现任何其他异常，也立即停止，不用
   重复次数替代触发条件。只有得到故障窗口内的证据差异，才重新评估假设 2/4 或 i3/i4 A/B。

## 参考

- [Linux v6.12 `drm_atomic_helper_resume()`](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_atomic_helper.c#L3585-L3618)：
  reset software state 后提交 suspend 时保存的完整 duplicated atomic state。
- [Linux v6.12 Rockchip DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/rockchip/rockchip_drm_drv.c#L241-L260)：
  master resume 交给 mode-config helper，没有在 state replay 后再逐 CRTC 恢复 cursor。
- [Linux v6.12 Tegra DRM PM](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/tegra/drm.c#L1340-L1357)：
  同样由 mode-config helper 完成 master suspend/resume。

上述为恢复责任边界的社区对照，不是 FH2M 红屏的直接同型修复。
