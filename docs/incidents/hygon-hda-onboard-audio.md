# Hygon 板载声卡

## 现象

板载 HDA 控制器是 PCI `1d94:14c9`（本机 `0000:06:00.6`）。`snd-hda-intel` 只按
`azx_ids[]` 绑定，没有厂商通配。表里没有这个 ID 时模块可以加载，但只剩下 GPU 的
HDMI 音频，板载扬声器、耳机和麦克风都不出现。

## 修复

[`components/linux/004-snd-hda-hygon-14c9.patch`](../../components/linux/004-snd-hda-hygon-14c9.patch)
在 6.12 的 `sound/pci/hda/hda_intel.c` 里，兆芯 `0x3288` 之后、龙芯条目之前加入：

```c
{ PCI_VDEVICE(HYGON, 0x14c9), .driver_data = AZX_DRIVER_GENERIC },
```

`PCI_VENDOR_ID_HYGON` 已是 `0x1d94`。编解码器仍由 `snd-hda-codec` 识别，这里不做机型特例。
`CONFIG_SND_HDA_INTEL=m`，可以只编译 `sound/pci/hda` 后
`modprobe -r snd-hda-intel && modprobe snd-hda-intel`，不必为这一项重启。

适用基线是 Gentoo `gentoo-sources` `6.12.111` 的 `sound/pci/hda/hda_intel.c`。
SHA-256 为 `08dc3079d5b6d8d698afe321b957ae17ec410f269ab379ab1673c67e1891a229`。

6.18 不能套用这份补丁。控制器源码改到了 `sound/hda/controllers/intel.c`，而且兆芯
`0x3288` 和龙芯条目之间多了几条 HDMI ID。要支持 6.18 必须按那棵树重做 hunk。

## 本机观察（2026-10-12，boot `0add59db-b33f-450a-9c07-efee7df6bef6`）

- `0000:06:00.6` 的驱动是 `snd_hda_intel`。
- `/proc/asound/cards` 的 card 1 是 `HD-Audio Generic`。
- `dmesg` 里 `snd_hda_codec_conexant` 把 SN6180 配成 speaker、headphone、Mic 和 Internal Mic。
- 本轮没有做听音。

## 回退

`--fuzz=0 -R` 卸下 004，重新安装 `snd-hda-intel` 模块。源码字节若与已安装模块一致，
不必为了“重打同一补丁”再编译一次。
