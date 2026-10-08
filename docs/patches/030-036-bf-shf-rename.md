# 030-036：111 内核私有位移宏改名

## 问题

Debian `6.12.111` 的 `include/linux/bitfield.h` 把 `__bf_shf` 从带参宏改成对象式
`#define __bf_shf __builtin_ctzll`。i12 的 `fantgpu/fant_math.h` 仍定义带参宏
`__bf_shf(x)`，DKMS 在 `-Werror` 下失败。`6.12.107` 的同名宏仍是带参形式，所以这条冲突是 111 新增的。

## 修复

`patches/030-036.patch` 只把驱动私有宏改名为 `__fant_bf_shf`。表达式保持
`(__builtin_ffsll(x) - 1)`。`FANT_FIELD_PREP` 与 `FANT_FIELD_GET` 的两处引用一起改名。
不使用 `#undef` 或 `#ifndef`，因此不随包含顺序改写内核宏，也不改零掩码上的原语义。

父树是锁定的 `5.0.0-i12` 终树 `9a8d185f2a65892a585b8f8f699f849a46ac6a4ab3c3e39a6a3a09357c2eb2ee`
（其中已含 030-035 与 i8–i11 派生）。严格应用后的树哈希是
`0eda30cdce3da3d872c56e7ebb3c89b4dc23a5fead402f0e43300b2130a184ce`。
本补丁不进入 `scripts/build-innogpu-driver.sh`，i12 终树门保持不变。重装未带本补丁的 DKMS 包会回到旧头文件，需要重新应用本补丁。

`fantgpu/fant_math.h` 与锁定 i6 快照中的同名文件字节相同。单元测试因此在 i6 快照上回放，
并单独钉住 i6 回放树哈希 `ae65641f4f9da16eb5fee7f92249d4db282db952320ca5aadf36c2944c243082`。
该哈希不是 i12 部署树哈希。

`drivers/innogpu/inno_math.h` 是 D 血缘，不是这条 DKMS 编译对象，不在本补丁内。

## 证据与边界

静态入口：

```bash
bash tests/unit/run-030-036-bf-shf-rename-tests.sh
```

111 上的 DKMS、ABI 与 001/002 重编结果记在 R52 任务段 2，不改写本文件来冒充运行 PASS。
不触发 PM，不改 GRUB 默认，不改 1C，不签 validation-results，不打 tag。

回滚是在派生树上反向应用本补丁，或丢弃派生树并回到 i12 终树。不改 i6 快照，不改 030-035。
