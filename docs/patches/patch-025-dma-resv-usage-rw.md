# patch-025：CPU_PREP 的 dma_resv usage 语义修复

## 目的

修复 `inno_gem_object_cpu_prep_ioctl()` 把 `bool write` 直接作为
`enum dma_resv_usage` 传入的问题。这是内核 API 语义错误，不是性能优化。

## 背景

- 位置：`innosrvkm/innodpu_drm_gem.c` `inno_gem_object_cpu_prep_ioctl()`
  （第 2602 行 `dma_resv_wait_timeout_rcu`、第 2608 行 `dma_resv_test_signaled_rcu`）。
- Linux 5.19+ 的 `dma_resv_wait_timeout()`/`dma_resv_test_signaled()` 第二参数从
  `bool write` 改为 `enum dma_resv_usage`；本项目经 `innodpu_compatibility.h` 把
  `_rcu` 变体映射到无 `_rcu` 版本后，`write` 被隐式转换成 usage 枚举。
- 语义错误：

| CPU 请求 | 当前传值 | 实际 usage | 应为 | 后果 |
| --- | --- | --- | --- | --- |
| READ（write=false） | `false`=0 | `DMA_RESV_USAGE_KERNEL` | `DMA_RESV_USAGE_WRITE` | 只等 KERNEL fence，漏等 WRITE fence，CPU 读可能与 GPU 写并发 |
| WRITE（write=true） | `true`=1 | `DMA_RESV_USAGE_WRITE` | `DMA_RESV_USAGE_READ` | 只等 WRITE fence，漏等 READ fence，CPU 写可能与 GPU 读并发 |

- 正确做法：`dma_resv_usage_rw(write)`（读请求等 WRITE usage、写请求等 READ usage，
  语义见内核 `dma-resv.h` 注释）。

## 已知边界（必须如实记录）

- **不是性能修复**：WebKit DMA-BUF 调查中"模拟正确 READ reservation usage"的受控 A/B
  （42.47% 对 42.80% CPU）显示本修复对该路径 CPU 无直接改善，见
  [webkit-dmabuf-investigation.md](../planning/webkit-dmabuf-investigation.md)。
- 本修复是**正确性修复**：使 CPU 访问前等待语义与 dma_resv 契约一致，消除与 GPU
  并发访问的潜在竞态；后续若有 fence 生命周期问题可在此基础上定位。

## 补丁内容

1. `innosrvkm/include/innodpu_compatibility.h`：在现有 dma_resv 兼容段后增加版本守卫助手：

   - `DRM_VERSION >= 5.19`：`innodpu_dma_resv_usage_rw(write) = dma_resv_usage_rw(write)`
   - 否则：`innodpu_dma_resv_usage_rw(write) = (write)`（旧内核仍是 bool 签名，保持原行为）

2. `innosrvkm/innodpu_drm_gem.c`：两处调用改用 `innodpu_dma_resv_usage_rw(write)`。

只改这两个文件的这两处；不混入其他变量（vblank、foreign DMA-BUF、预取均不在本补丁）。

## 构建开关

`APPLY_DMA_RESV_USAGE_FIX=1`（建议默认启用；候选包为 patched-25 或后续版本）。

## 验证与回退

- 离线验证：在 Deepin 202504 原包 DKMS 源码上叠加本补丁，对 `6.12.101+deb13-amd64`
  headers 编译 `innogpu.ko` 成功且 vermagic 匹配；不安装、不热切换。
- 实机验证门槛（由操作者在真实会话执行）：
  1. 安装候选包并重启，确认 DKMS、Driver/Firmware、DRM/fbdev 正常；
  2. 最小 PDP 探针 READ/WRITE CPU_PREP 行为回归（`tools/probe-pdp-invisible-read.c`）；
  3. 桌面 Xorg/GLX 与既有 p21/p24 行为一致。
- 回退：直接回退点 `patched-24`；本补丁不改变内存布局或 ioctl ABI。
- 若新内核（>6.12）或旧内核（<5.19）构建失败，先检查 `innodpu_compatibility.h` 的
  版本分支，不绕过编译错误。

## 参考

- 内核 `include/linux/dma-resv.h`：`enum dma_resv_usage` 与 `dma_resv_usage_rw()` 定义。
- [webkit-dmabuf-investigation.md](../planning/webkit-dmabuf-investigation.md) 静态审计第 2 项。
