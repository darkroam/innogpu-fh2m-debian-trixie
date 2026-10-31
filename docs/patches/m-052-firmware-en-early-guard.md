# m-052-firmware-en-early-guard：firmware_en=0 时跳过封闭 matcher

`firmware_en=0` 时封闭对象仍报 hwinfo 完成，所以 `m-030` 挡不住。
Debian 的厂商 initramfs 钩子会在根文件系统挂载前加载 `fantgpu`。
`fh2m_hal_os_release_match()` 这时读发行文件会崩溃。

`patches/m-052-firmware-en-early-guard.patch` 只改 `fantsrvkm/fantaudio_drv.c`。
`!firmware_en` 时直接用 `DMA_POINTER_NORMAL` 返回，不挪 m-030 的校验。
值为 0 且系统本应匹配 NFSDesktop 或 SeewoOs 时，模式是 NORMAL，不是 DMAIRQ 或 HWTIMER。
项目默认是 0，目标是 Debian 与 Gentoo，这个差别可接受。
封闭对象内部没有逐行复核。依据是现场崩溃链，以及同一守卫后三核启动通过。

本补丁不进入 `derive_fantgpu_source`。Debian 包源在派生后打 `m-036`、`m-051`、`m-052`。
Gentoo 在 `m-051` 之后打它。2026-10-31 在 `6.12.111-gentoo-x86_64` 干净构建两次，srcversion 都是 `57FE1935E2D4FA68E637455`，未安装。现场模块仍是 `D579486A64C340B5178B66B`。i12 tag 与签署件不动。`117/195` 仍单列。

```
bash tests/unit/run-m-052-firmware-en-early-guard-tests.sh
```

回滚是在已含 m-030 的树上反向应用本补丁。不改 m-030。
