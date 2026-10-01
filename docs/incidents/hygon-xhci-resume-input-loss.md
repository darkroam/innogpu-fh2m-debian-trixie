# Hygon xHCI 恢复后外接键盘失效

## 现象

2026-10-01，`5.0.0-i12` 的首次 `pm_test=devices` 返回 `rc=0`，GPU DVFS suspend/resume
也均为 `rc=0`，但 Corsair K70 外接键盘恢复后不能输入。同一控制器 USB3 companion 树上的
RTL8153 随即记录 `status -71`；用户短按电源后，关机画面停在 `r8152` reset 附近。

## 根因与修复

键盘 `1b1c:1b36` 和网卡 `0bda:8153` 均位于 Hygon xHCI 控制器 `1d94:148c`
（`0000:06:00.1`）下。原控制器 quirk 为 `0x10`，恢复时保存状态未能完整还原。该故障独立于
已经修复并正常返回的 fantgpu DVFS 路径。

[`components/linux/001-hygon-148c-xhci-reset-on-resume.patch`](../../components/linux/001-hygon-148c-xhci-reset-on-resume.patch)
只对该 PCI ID 启用 Linux xHCI 已有的 `XHCI_RESET_ON_RESUME`，不修改 fantgpu、不扩展到其他
Hygon ID，也不使用用户态 USB rebind。

上游基线为 Debian `linux-source-6.12` `6.12.107-1` 的
`drivers/usb/host/xhci-pci.c`，基线 SHA-256 为
`fcc0de63b9949150529e8458e0ae5f6f3f1377e1de18b1f84e5d9f021b51ec0a`。归档补丁
SHA-256 为 `299eb3f3d688a48518278c1ac5c6e97be89c8cc8f2c50bdcbfc4881bd47a5614`。

## 验证与边界

测试模块重启后 quirk 为 `0x90`。`pm_async=1` 下连续三次 `pm_test=devices` 均 `rc=0`，
每次恢复后 Corsair HID 仍绑定 `usbhid`、RTL8153 仍绑定 `r8152`，键盘 LED 事务成功，日志无
`-71`、USB 枚举失败、xHCI timeout 或 `propagated=-14`。三轮后用户通过外接键盘写入
`/tmp/r49-external-keyboard-ok`；内容时间为 `2026-10-01T17:56:15,763910243+08:00`，文件
SHA-256 为 `bb092bf391e4f937d6126d1c687855899b6fb4588a80665583ef99e9c23a40dc`。
归档升级门于 `2026-10-01T18:09:43+08:00` 使用特权只读 boot 日志复核，
`0000:06:00.1` 与 `0000:06:00.2` 均返回 `PASS quirks=0x0000000000000090`。

原始证据留在本机 `.build/r49-xhci-fix-20261001/`，不随 Git 分发。归档只保存 GPL-2.0-only
派生补丁与必要摘要，不保存完整 Linux 源码、模块、initramfs、签名材料或安装脚本。正常 deep
在该分支未运行，因此这项修复及 3/3 devices 结果不能单独完成 R49 原定的 3+1 验收，也不改变
R5 的 `FAIL` 状态。

## 内核与系统升级检查

该修复目前不是发行版内核的一部分。每次安装新内核、升级系统或迁移机器后，必须在任何 suspend
验收前运行 `sudo scripts/check-hygon-xhci-resume-fix.sh`。脚本只读检查目标 PCI ID 和当前 boot 的
xHCI quirk 日志：`PASS` 才证明 `0x80` reset-on-resume 位生效；`FAIL` 表示新内核未包含修复；
`UNVERIFIED` 表示日志不足，二者都不得继续 PM 验收。升级后的内核若已由上游包含等价 quirk，
可直接通过该检查，不要求继续使用本地 `updates/r49` 模块。

## 回退

已安装测试模块位于 `/lib/modules/6.12.107+deb13-amd64/updates/r49/xhci-pci.ko.xz`。现场回退
脚本保存在 `.build/r49-xhci-fix-20261001/rollback.sh`；移除覆盖模块并恢复该内核 initramfs 后
重启即可回到发行版 xHCI 驱动。执行回退仍属于宿主修改，必须另行获准。
