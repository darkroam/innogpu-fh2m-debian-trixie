# Stage 000：跳过首次 G0M GPU PLL 设置

## 目的

规避 Innogpu 3.3.3.42 在 Debian Trixie 6.12 上进入
`g0m_soc_hw_init -> g0m_soc_setpll -> set_pll_reg` 时的已知内核 Oops。

## 实现

- 目标是 Deepin 202504 原包中的 `innogpu/innogpu.o_shipped`，不是历史 patched 包中的 `.ko`。
- `tools/patch-gpupll-object.py` 要求目标调用字节 `e8 09 fd ff ff` 恰好出现一次，并替换为五个 NOP。
- `scripts/build-deepin-coherent.sh` 在应用源码补丁后、DKMS 打包前无条件执行该变换。
- `scripts/patch-skip-first-gpupll.sh` 复用同一工具，只用于受控修复源码对象或已安装模块。

该阶段修改厂商预编译对象，无法表示为普通源码 diff，因此使用独立、可审查的确定性工具，并在补丁
索引中作为 stage-000 明示；这不是绕过构建失败的临时命令。

## 验证

- 原字节序列必须唯一，否则工具拒绝修改。
- 已替换目标可幂等识别，构建不会重复改变对象。
- patched-19/20 均从 Deepin 202504 原对象执行该阶段并完成 DKMS 构建。
- patched-20 已通过重启、PVR、Xorg/GLX、fbdev 和真实 VT 验收。

## 边界与回退

该字节契约只适用于当前锁定的 Innogpu 3.3.3.42 预编译对象。升级厂商对象时必须重新定位调用点、
反汇编确认语义并更新工具；禁止在新对象上放宽为“找到任意相似字节就修改”。回退时重新从 Deepin
原包解包未修改对象并完整重建，不从历史 `.ko` 复制片段。
