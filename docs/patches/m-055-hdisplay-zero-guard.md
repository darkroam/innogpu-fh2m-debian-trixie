# m-055-hdisplay-zero-guard：hdisplay==0 或 vdisplay==0 不缩放

`patches/m-055-hdisplay-zero-guard.patch` 在 `pdp0_crtc_atomic_check_scaling()` 的两处 `div_u64` 之前加守卫。条件是 `adjusted_mode.hdisplay == 0` 或 `adjusted_mode.vdisplay == 0`。命中时写 `scale_enable=false`、`enhancer_enable=false`，然后 `goto mclk_calc`。不返回 `-EINVAL`，也不把除数钳成 1。

combine 宽度路径要求 `hdisplay>2048`，与这个守卫互斥，已验证的宽屏路径行为不变。`FANT_PDP_COMBINE` 置位但 `hdisplay==0` 时不再落入 else 后除 `crtc_w`。`vdisplay==0` 时不再走到 `:770` / `:773` 的垂直除法。

`enhancer_enable=false` 对齐原 `:754` 在输出为 0、源非 0 时会写入的值。守卫不跑 `div_u64`。不落到后面的 `scale_enable=true`。

已知未拦形态，可达性待证：`src==0` 且 `crtc_w!=0` 时，`div_u64` 仍除 `src_w`。不另加守卫。这不是整类除零已修。

不进 derive。Debian 包源和 Gentoo 构建链都在 `m-054` 之后、`g-053` 之前打上。`in_i12_builder` 为 false。Gentoo `6.12.115` 两次干净构建 srcversion 都是 `B72875836C2992A4FD97B66`，字节不同。磁盘与本次加载都是第二次 `1370b91b459540af98180e6f2bdbea7067b0a5c601e12cf54f43ad8659108bb2`。boot `ba2bdc9e-fefc-47e1-9f90-6ab29fd1b306`。`g-053` 之后的源码树是 `6efe6ffc56a33174f65b9262aaa2fd03d4b362261bc01e14294af981037d2031`。`fb0014bd` 不是编完后的树。只改 115 行身份。

```bash
bash tests/unit/run-m-055-hdisplay-zero-guard-tests.sh
```

## 六项验证

2026-11-25 在 Gentoo `6.12.115-gentoo-x86_64` 上跑过。boot `ba2bdc9e-fefc-47e1-9f90-6ab29fd1b306`。加载与磁盘 srcversion 都是 `B72875836C2992A4FD97B66`。`check-kernel-patches.sh` 与 `verify.sh` 都是 PASS。八项计数 0。`panel_backlight` 在。`firmware_en=0`。请求行 0。`117 not found` 5 次，`195 not found` 1 次。

1. 规避开关开着时合盖再开盖，内屏不断，没有 Oops。这不是修复证明。
2. 开关改为 false 后，16:00:18 xwayland 只剩 `DP-1`。用户能开终端，鼠标进不了内屏。dmesg 没有这次断连行，也没有 `divide error`、`Oops`、`BUG`。
3. 16:00:30 内屏回到 `2560x1600`。开盖后 `eDP-1` 与 `DP-1` 都是 connected。
4. 合盖和开盖期间外接 `DP-1` 一直出图。
5. 就是这棵 `6.12.115`。两次干净构建同 srcversion 后，只把 115 行改成这个身份。
6. 没有第二棵目标内核，不另编。EDID 0 字节不列入通过条件。

`src==0` 且 `crtc_w!=0` 仍未拦，可达性待证。

## 厂商报告

除零在开放 `fantsrvkm/pdp0_crtc.c` 的 `pdp0_crtc_atomic_check_scaling()`，不在 `fantgpu.o_shipped`。`pdp0_plane.c` 把 `crtc_w` 写成 `adjusted_mode.hdisplay`。断开 eDP 时 hdisplay 为 0 仍会进入缩放，然后除 `output_w`。`vdisplay==0` 会除 `output_h`。

`g3_pdp0_hw.c:78-88` 在 `dpu_id` 为 0、2 或 4 且 `s_combi_dual_sel` 对应位为真时置 `FANT_PDP_COMBINE`，并把 `max_width` 设为 4096 或 7680。else 不置该位。置上后，只有 `hdisplay>2048` 才改用 `crtc_w>>1` 和 `src_w>>1`。`hdisplay==0` 即使该位置上，也走 else 并除 `crtc_w`。

本补丁在 `div_u64` 之前跳过 `hdisplay==0` 或 `vdisplay==0`。不要钳成 1，也不要用合盖选项代替修复。

已知未拦形态，可达性待证：`src==0` 且 `crtc_w!=0` 时，`div_u64` 仍除 `src_w`。不要把本补丁读成整类除零已修。
