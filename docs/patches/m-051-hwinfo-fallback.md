# m-051-hwinfo-connector-fallback：hwinfo 缺失时的连接器回退

## 问题

没有 `hwinfo_g0m.bin` 时，`fantdpu_get_output_index` 和背光查询每次都读 version，再对每个连接器打 ERROR。HDMI 查询失败后调用方把下标改成 0，所以选中的仍是表项 0。本机 DP0 内屏已经在 version 失败时返回 index 9，不能改成 DP。

闭源对象里的 `skip hwinfo` / `register no pass` 不在这个补丁里。`fantdpu_hdmi.c` 的 refclk 查询和 `fantdpu_dp_common.c` 的 `DP_ANACFG` 也不改。probe 到 fbcon 没有第二段可合并的硬件 modeset。

## 修复

`patches/m-051-hwinfo-connector-fallback.patch` 只改 `fantsrvkm/fantdpu_connector.c`。

- version 和 output index 按设备和连接器缓存，只查一次。
- version<0 时 `fant_printk_once` 一条说明。DP0 内屏仍返回 9。其余连接器返回 0，不读 `s_hdmi_dvi` / `s_dp_vga`，不再返回 `-EFAULT`。
- 原来的四条逐连接器 ERROR 改成缓存未命中时的 info，或并进这一条说明。背光仍返回 DDCCI。

父树是 `m-036-bf-shf-rename` 之后的 i12 树 `0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce`。
严格应用后的树哈希是 `b0a46c345c1e0d93baf51c554a1cc9dde0757feb68ce4a9951c3c01458f71c00`。
本补丁不进入 `tools/debian/build-driver.sh`。Gentoo 后备构建在 `m-036-bf-shf-rename` 哈希通过后应用它。

## 证据与边界

```bash
bash tests/unit/run-m-051-hwinfo-fallback-tests.sh
```

冷启动窗口和开放 ERROR 次数记在 R58，不在这里写成已达标。不触发 PM，不改黑盒字节。

回滚是在 `m-036-bf-shf-rename` 树上反向应用本补丁。不改 i6 快照，不改 `m-036-bf-shf-rename`。
