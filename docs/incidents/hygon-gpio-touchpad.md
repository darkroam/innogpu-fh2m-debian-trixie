# Hygon GPIO 与触摸板

## 现象

KaiTian X7h G1e 的 GPIO 控制器 ACPI ID 是 `HYGO0030`。上游 `amd_gpio` 只认
`AMD0030` / `AMDI0030` / `AMDI0031`，于是没有驱动绑定这个控制器。触摸板
`HTIX5288` 的中断挂在 GPIO 上，I2C HID 探测一直 defer，触摸板不出现。

## 修复

[`components/linux/003-pinctrl-amd-hygon-gpio.patch`](../../components/linux/003-pinctrl-amd-hygon-gpio.patch)
只在 `drivers/pinctrl/pinctrl-amd.c` 的 `amd_gpio_acpi_match[]` 末尾加一项
`HYGO0030`。海光控制器与 AMD GPIO 兼容，驱动逻辑不改。

适用基线是 Gentoo `gentoo-sources` `6.12.111`（genpatches 123）。SHA-256 为
`2f6676f38175a709bb71488de5caa7e8490bf75e17d3a4e21c2b0f4b286ced0e`。
6.18 的同一张表形状相同，但那棵树不在本批重打范围内，不能把本补丁的验收直接搬过去。

配置：`CONFIG_PINCTRL_AMD=y`（bool，不能编成模块）、`CONFIG_I2C_HID_ACPI=m`、
`CONFIG_HID_MULTITOUCH=m`。改内建项后必须整核重编并重启。

## 本机观察（2026-10-12，boot `0add59db-b33f-450a-9c07-efee7df6bef6`）

- `/sys/bus/platform/drivers/amd_gpio/` 下有 `HYGO0030:00` 和 `HYGO0030:01`。
- `xinput` 有 `HTIX5288:00 36B6:C002 Touchpad`。
- 本轮没有重新做手指滑动。启动日志里有一串 `i2c_hid_get_input: incomplete report`，
  触摸板仍然完成了 input 注册；这串日志不另立修复。

## 回退

`--fuzz=0 -R` 卸下 003，重新编译并重启。只改源码、不重编的话，正在运行的内核不会变。
