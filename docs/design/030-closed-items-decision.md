# 关闭项决策批：003 / 004 / 005 / 008 / 025-display + patch-000（F0 侧终判）

- 日期：2026-09-10
- 依据：030-mapping-table §三.3（D015-D019）+ §三.1（追溯 BC）+ P3c 裁决口径（retain / runtime-verify）+ dsh 放行规格（关闭依据逐项落档）
- 范围：5 个关闭项台账 + patch-000 构建期对象变换的 F0 侧处置；**不新增 030-NNN**，本批为决策归档文档
- 判定方法：每项对比「D_base 语义」与「F0 基线语义」；D_stage 未应用的 patch 在 F0 侧无迁移增量时判 retain；i4-only 项判 excluded + runtime-verify
- **行号引用基准**：本文所有行号均出自 **F0 基线树**（O-4 锁定 `7219d817…`，未应用任何 030-NNN；= f0-snapshot.tar.zst 解包树），与 030 链后树行号有偏移属预期，特此注明

## 一、逐项终判

### 1. 003 — panel-backlight-fallback（D015，P5 line 367，2 项）

| 字段 | 值 |
| --- | --- |
| 来源 patch | `patches/003-panel-backlight-fallback.patch`，SHA `8cd6b492b01e2c42…` |
| D 侧处置 | **不应用**（patched-27 集合外） |
| 语义 | ① innodpu_get_connector_backlight_mode 各失败路径回退 DDCCI→PWM0 + 末尾 DDCCI→PWM0 强制重映射；② s_bl_en_ctrl 默认 false→true + 不支持的背光模式回退 PWM0（REG_ENTITY0000-0003 直写） |
| **F0 侧终判** | **retain（无迁移增量）** |
| 依据（行级） | F0 `fantsrvkm/fantdpu_connector.c:2301-2321` `fantdpu_get_connector_backlight_mode` 各失败路径回退 DDCCI（= D_base 语义，无末尾重映射）；`fantsrvkm/fantdpu_panel_backlight.c:54` `s_bl_en_ctrl = false`（= D_base）；`:1064-1065` 不支持模式 `dev_warn + return -EINVAL`（= D_base，无 PWM0 回退） |
| 追溯 | BC-09×2（srvkm/dpu_connector.c、srvkm/dpu_panel_backlight.c）；D_stage 未应用 → F0 保持 D_base 同态即可 |

### 2. 004 — panel-platform-fallback（D016，P5 line 368，2 项）

| 字段 | 值 |
| --- | --- |
| 来源 patch | `patches/004-panel-platform-fallback.patch`，SHA `330c3a06998400bb…` |
| D 侧处置 | **不应用**（关闭项） |
| 语义 | ① panel_pwr_create 的 G1/G1P 平台 warn+NULL → fallthrough GPIO 回退（含 G1_PAL/NE/G1P_PAL/NE/G0_PAL/NE/G0M_PAL/NE）；② panel_backlight_init 平台 fallthrough + 未知平台 PWM 回退 |
| **F0 侧终判** | **retain（无迁移增量）** |
| 依据（行级） | F0 `fantsrvkm/fantdpu_panel_pwr.c:598-599` G1/G1P `dev_warn + return NULL`（= D_base，无 fallthrough）；`fantsrvkm/fantdpu_panel_backlight.c:1059-1061` 未知平台 `dev_warn + return -EINVAL`（= D_base） |
| 追溯 | BC-09×2（srvkm/dpu_panel_pwr.c、srvkm/dpu_panel_backlight.c）；D_stage 未应用 |

### 3. 005 — backlight-force-initial-enable（D017，P5 line 369，1 项）

| 字段 | 值 |
| --- | --- |
| 来源 patch | `patches/005-backlight-force-initial-enable.patch`，SHA `9fee230ceb3347b0…` |
| D 侧处置 | **不应用**（关闭项） |
| 语义 | register_bl 处强制 default_brightness=max_brightness、props.power=FB_BLANK_UNBLANK、state=0，注册后 backlight_update_status 立即点亮 |
| **F0 侧终判** | **retain（无迁移增量）** |
| 依据（行级） | F0 `fantsrvkm/fantdpu_panel_backlight.c:1068-1085` register_bl 段无上述三项强制逻辑（= D_base 语义）；F0 自有 default_brightness 夹取（`:963-964` `max_brightness / 2` 50%）为 vendor 基线行为，与 patch 语义无关，保留不动 |
| 追溯 | BC-09（srvkm/dpu_panel_backlight.c）；D_stage 未应用 |

### 4. 008 — pvr-init-diagnostic（D018，P5 line 370，1 项）

| 字段 | 值 |
| --- | --- |
| 来源 patch | `patches/008-pvr-init-diagnostic.patch`，SHA `4cfd545afcfbac33…` |
| D 侧处置 | **不应用**（关闭项） |
| 语义 | srvkm_init ioctl 前后 pr_info 诊断（module/priv/dev_node/state_before/after） |
| **F0 侧终判** | **retain（无迁移增量）** |
| 依据（行级） | F0 生效实现 `fantsrvkm/fantgpu_drm.c:217-244` `drm_ft_srvkm_init` 无 state_before/state_after 诊断日志（= D_base 语义）；`fantsrvkm/ft_drm.c:397` `#if 0` 内的同名旧实现（399 起）为死代码，不计入判定依据 |
| 追溯 | BC-09（srvkm/gpu_drm.c → F0 对应生效文件为 **fantsrvkm/fantgpu_drm.c**，非 ft_drm.c） |

### 5. 025-display — suspend-resume-display（D019，P5 line 371，1 项）

| 字段 | 值 |
| --- | --- |
| 来源 patch | `patches/025-suspend-resume-display.patch`，SHA `dfd251570d12d43c…` |
| D 侧处置 | **i3 不应用**（i4-only；根因假设未证实/证伪） |
| 语义 | innodpu_drm_resume 删除 per-CRTC `innodpu_pdp0_wakeup` 循环 |
| **F0 侧终判** | **excluded + runtime-verify（维持排除，UNVERIFIED 登记）** |
| 依据（行级） | F0 `fantsrvkm/fantdpu_drm_pm.c:288-304` `fantdpu_drm_resume` 含 `drm_for_each_crtc + fantdpu_pdp0_wakeup` 循环（`:299-300`，= D_base/i3 语义）；patch 语义（删除循环）为 i4-only 根因假设，未证实/证伪 → 不进 030 序列 |
| UNVERIFIED 登记 | O_stage 整体验收矩阵：suspend/resume 实机观察（tools/probe-suspend-resume-state.sh 既有 fixture），若出现 i4 假设所针对的唤醒显示异常再行裁决（属 O_stage 运行时修复范畴，不属 030-NNN 重放） |
| 追溯 | BC-09（srvkm/dpu_drm_pm.c）；mapping 表「i3 不应用 + runtime-verify 口径」 |

## 二、patch-000（构建期对象字节变换）F0 侧处置

| 字段 | 值 |
| --- | --- |
| D 侧语义 | 基线导入时对 `innogpu/innogpu.o_shipped`（3.3.3.42 HAL 对象）执行 `tools/patch-gpupll-object.py`：将首个 G0M GPU PLL setup 调用的 `e8 09 fd ff ff` 字节 NOP 化（构建期确定性变换） |
| **F0 侧终判** | **no-transform（操作结论）**：F0 基线不执行该字节替换，对象原样使用（O-4 已锁定）。**注意：本结论仅指「不执行目标字节替换」这一操作，不构成「F0 不存在 G0M GPU PLL 双重初始化风险」的语义闭合**——语义风险已登记 UNVERIFIED，见下 |
| 依据 | ① F0 HAL 对象 = `fantgpu/fantgpu.o_shipped`（3.3.8.126 构建，6,903,704 字节，O-4 已锁定），字节模式 `e8 09 fd ff ff` **零命中**——D 侧变换目标模式在 F0 对象中不存在；② 跨构建二进制间字节偏移不可比对/不可外推（不同 vendor 构建的重编译使该 rel32 调用位移全局漂移），D 侧字节变换不可移植；③ F0 全树 o_shipped 扫描：fantgpu/fantdma/fantsmmu/fantvpu 零命中；`fantsrvkm/fantsrvkm.o_shipped` 单次命中为不同对象中的无关 call 指令（位移巧合，非 HAL PLL 调用），不做处理 |
| UNVERIFIED 登记（语义结论未闭合） | 若实机出现 G0M GPU PLL 双重初始化症状（阶段三验证矩阵可观测），再行裁决（O_stage 运行时修复范畴）；在实机验证完成前，该语义风险维持 UNVERIFIED，不得以 no-transform 操作结论替代语义判定 |
| 追溯 | 台账 stage-000/D001（基线导入 + 构建期确定性变换）；F0 侧对应处置 = 声明无需变换 + 本归档 |

## 三、批次结论

- 5 个关闭项 + patch-000 全部无 F0 迁移增量：**retain ×4（003/004/005/008）、excluded + runtime-verify ×1（025-display）、no-transform ×1（patch-000）**；
- 不新增 030-NNN；O_stage 树不变（= 030-029 after `937e3710…`）；
- UNVERIFIED 登记 2 条（025-display 唤醒显示观察、patch-000 PLL 双重初始化观察），并入 O_stage 整体验收矩阵；
- 下一阶段：O_stage 构建集成（builder/epoch/packaging 走 5.0.0-i1 系列）→ 阶段三验证矩阵 → 三方一致 + dsh 终审 + 用户批准 → 5.0.0-i1 tag。
