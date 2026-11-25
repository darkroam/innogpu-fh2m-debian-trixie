# m-055-hdisplay-zero-guard：hdisplay==0 或 vdisplay==0 不缩放

`patches/m-055-hdisplay-zero-guard.patch` 在 `pdp0_crtc_atomic_check_scaling()` 的两处 `div_u64` 之前加守卫。条件是 `adjusted_mode.hdisplay == 0` 或 `adjusted_mode.vdisplay == 0`。命中时写 `scale_enable=false`、`enhancer_enable=false`，然后 `goto mclk_calc`。不返回 `-EINVAL`，也不把除数钳成 1。

combine 宽度路径要求 `hdisplay>2048`，与这个守卫互斥，已验证的宽屏路径行为不变。`FANT_PDP_COMBINE` 置位但 `hdisplay==0` 时不再落入 else 后除 `crtc_w`。`vdisplay==0` 时不再走到 `:770` / `:773` 的垂直除法。

`enhancer_enable=false` 对齐原 `:754` 在输出为 0、源非 0 时会写入的值。守卫不跑 `div_u64`。不落到后面的 `scale_enable=true`。

已知未拦形态，可达性待证：`src==0` 且 `crtc_w!=0` 时，`div_u64` 仍除 `src_w`。不另加守卫。这不是整类除零已修。

不进 derive。Debian 包源和 Gentoo 构建链都在 `m-054` 之后、`g-053` 之前打上。`in_i12_builder` 为 false。尚未干净构建，不改任何已验收 srcversion。两次干净构建同 srcversion 之前不动矩阵身份。

```bash
bash tests/unit/run-m-055-hdisplay-zero-guard-tests.sh
```

## 六项验证

未跑。实机由用户配合。验收第 2 项时关掉 `keep-laptop-panel-on-when-lid-is-closed`。

1. 合盖不断 eDP：规避开关打开，无 Oops，eDP 仍连接。这不是修复证明。
2. 合盖断 eDP：规避开关关闭，外接 DP 在。断开提交成功，无 `divide error`、`Oops`、`pdp0_crtc_atomic_check`。
3. 开盖恢复：eDP 回到真实模式。
4. 外接 DP：第 1 到第 3 步保持出图。
5. `6.12.115`：修前对照 `0xa5a17` / `0xa5a2d` 仍是 `41 f7 f6`。修后两次干净构建同 srcversion 才记新身份。
6. 目标内核：若不是 `6.12.115`，同一补丁再构建并做第 2 项。EDID 0 字节不列入通过条件。

## 厂商报告

除零在开放 `fantsrvkm/pdp0_crtc.c` 的 `pdp0_crtc_atomic_check_scaling()`，不在 `fantgpu.o_shipped`。`pdp0_plane.c` 把 `crtc_w` 写成 `adjusted_mode.hdisplay`。断开 eDP 时 hdisplay 为 0 仍会进入缩放，然后除 `output_w`。`vdisplay==0` 会除 `output_h`。

`g3_pdp0_hw.c:78-88` 在 `dpu_id` 为 0、2 或 4 且 `s_combi_dual_sel` 对应位为真时置 `FANT_PDP_COMBINE`，并把 `max_width` 设为 4096 或 7680。else 不置该位。置上后，只有 `hdisplay>2048` 才改用 `crtc_w>>1` 和 `src_w>>1`。`hdisplay==0` 即使该位置上，也走 else 并除 `crtc_w`。

本补丁在 `div_u64` 之前跳过 `hdisplay==0` 或 `vdisplay==0`。不要钳成 1，也不要用合盖选项代替修复。

已知未拦形态，可达性待证：`src==0` 且 `crtc_w!=0` 时，`div_u64` 仍除 `src_w`。不要把本补丁读成整类除零已修。
