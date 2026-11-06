# m-054-efifb-kick-move：拆 efifb 挪到 fbdev 注册前

`patches/m-054-efifb-kick-move.patch` 删掉 `dev_rsrc_init()` 里的 `fh2m_fant_drm_fb_kick_off_efifb()`，改到 `fant_fbdev_create()` 中、`drm_fb_helper_initial_config()` 正前方。`!fh2m_hal_get_nulldisplay()` 仍把门。只踢一次。

6.12 上这一脚是 `aperture_remove_all_conflicting_devices()`，范围 `0 .. -1`，并 `sysfb_disable(NULL)`。名字参数不是选择器。`initial_config` 自己不拆 aperture，而且调用期间持有 `fb_helper->lock`，所以不把拆点放进 `fant_fbdev_probe()`。

先注册新 fb 再拆会停在 `fb1`。无 dummy 且仍占 `fb0` 要新的控制台交接，本补丁不做。`initial_config` 在拆点之后失败时，控制台留在 dummy；拆点之前失败则 efifb 还在。

拆点因此晚于 `dr_mtrr_setup()`。WC/uncached-minus 那行只是已知日志，不是已证明的因果。画面不回、卡住，或这行变成失败，就把调用放回 `dev_rsrc_init()`。

不进入 `derive_fantgpu_source`。Debian 包源和 Gentoo 构建链都在 `m-052` 之后、`g-053` 之前打它。`in_i12_builder` 仍是 false。Gentoo `6.12.113` 两次干净构建 srcversion 都是 `227E896A45E34187252CD2D`，2026-11-06 已加载。不顶替 111 的 `D579486A64C340B5178B66B` 或 Debian 行。iN 不动。

```
bash tests/unit/run-m-054-efifb-kick-move-tests.sh
```

实机还没做。过的判据是：`colour dummy` 只紧挨 `fb0: fh2mdrmfb`，空档明显短于 1.318 秒，注册行仍是 `fb0`，内屏黑短于约 5 秒，外屏不在约 9 秒重锁上再叠一次切换。journal 前段是积压，efifb 墙钟用 `dmesg -T`。两套钟都记。
