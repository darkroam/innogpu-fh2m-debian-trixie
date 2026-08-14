# fbterm YPan 显示错位与跨会话残留

## 现象

patched-21 已解决 `/dev/fb0 mmap()` 失败，fbterm 能进入并绘制，但进一步使用发现：长输入或大量
输出后，底部内容看似越出屏幕；执行 `clear` 后提示符不能稳定回到顶部；退出并重新进入 fbterm 后，
顶部残留行数还会增加。字体从 16 像素恢复为默认值后现象不变，因此字体不是根因。

## 证据与排除项

只读 framebuffer 探针得到：可见区域为 `1920x1080`，虚拟区域为 `2560x1600`，`ypanstep=1`，
`ywrapstep=0`。fbterm 内的 PTY 为 `274x77`；按默认约 `7x14` 字符单元计算，正好对应可见区域，
而不是虚拟区域。fbterm 1.7 的 `src/fbdev.cpp` 也明确使用 `vinfo.xres/yres` 计算屏幕尺寸，故不得
通过缩小驱动 `virtual_size` 或反复调整字体掩盖问题。

fbterm 看到虚拟高度和 `ypanstep` 后会选择 YPan，并以 `FBIOPAN_DISPLAY` 改变可见窗口偏移。当前
Innogpu fbdev 对 ioctl 返回成功，但该路径在持续滚动和重新进入时不能保持 fbterm 预期的显示语义；
原版 fbterm 的析构路径也没有显式把 `yoffset` 复位为 0。ioctl 成功只能证明接口接受请求，不能
证明加速滚动的像素结果正确。

另一个已确认但非本次像素错位根因的问题是：`TERM=linux` 的 `clear` 会追加 `CSI 3 J`，而 fbterm
1.7 的 `erase_display()` 仅处理参数 0、1、2。该差异会影响滚回历史的清除语义，但不能解释字符
网格尺寸，亦不能替代对 YPan 的受控对照测试。

## 修复

`patches/fbterm/001-configurable-redraw-scrolling.patch` 为 Debian fbterm 1.7-5 增加：

- `--scrolling=auto|redraw` 和同名配置项；默认 `auto` 保持上游行为；
- `redraw` 模式跳过 YPan/YWrap 探测，并在进入时把遗留 `yoffset` 归零；
- 使用加速滚动时，析构前也把 `yoffset` 复位为 0，避免偏移跨进程遗留。

本机 `~/.fbtermrc` 使用 `scrolling=redraw`。`scripts/build-patched-fbterm.sh` 从固定的 Debian
1.7-5 源码构建用户本地版本；默认关闭未使用的 GPM 和 legacy VESA 支持，系统
`/usr/bin/fbterm` 保持不变，可直接作为回退。

## 验证结果

2026-08-14 在真实 VT 完成两组对照：

1. 默认字体大小加 `scrolling=redraw`：大量输出、`clear`、长输入和退出后重新进入均正常；
2. 恢复 `font-size=16` 后重复上述操作：仍然正常。

该结果确认 redraw 是有效规避路径，字体仅改变行列密度。驱动仍会报告 YPan 能力，因此 stock
fbterm 的自动模式仍可能复现；是否在内核驱动中撤销或修复 YPan 能力，需要单独的驱动补丁和回归
验证，不能在本次用户态修复中顺带修改。

## 回退与后续门槛

- 临时绕过用户配置：运行 `/usr/bin/fbterm` 或把 `scrolling` 改为 `auto`；前者可能重新触发问题。
- 回退用户本地二进制：删除 `~/.local/bin/fbterm`，PATH 将重新选择 `/usr/bin/fbterm`。
- 后续若修改驱动 YPan，必须重复长输出、`clear`、至少两次退出/重入，并读取 `yoffset`；只验证
  `FBIOPAN_DISPLAY` 返回 0 不足以判定通过。
