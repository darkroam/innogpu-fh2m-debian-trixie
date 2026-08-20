# patch-027：foreign DMA-BUF 导入与 GTT 导出生命周期修复

## 目的

修复 PRIME/DMA-BUF 生命周期中的两个缺陷：
1. **导入路径类型混淆**：`innodpu_gem_prime_import()` 把任意 `dma_buf->priv` 直接当作
   `struct drm_gem_object` 解引用；且未检查 `dma_buf_attach()` 的 error pointer。
2. **GTT 导出映射泄漏**：`innodpu_gem_prime_map_dma_buf()` 的 GTT 分支对每页执行
   `dma_map_page()`，但 `innodpu_gem_prime_unmap_dma_buf()` 只释放 sg table，没有对应的
   `dma_unmap_page()`。

这是正确性修复（类型安全 + 资源泄漏），不是性能修复。

## 背景

### 1. 导入路径（`innosrvkm/innodpu_drm_gem.c` `innodpu_gem_prime_import`，约第 1170 行）

- 现状：`struct drm_gem_object *obj = dma_buf->priv;` 后直接 `if (obj->dev == drm_dev)`。
  对**本驱动导出的** dma_buf，priv 才是 drm_gem_object；对 foreign exporter（其他 DRM 驱动、
  V4L2、RDMA 等）的 dma_buf，priv 是对方私有结构，按 drm_gem_object 解引用属于类型混淆，
  可能崩溃或误判自导入。
- 另外 `attach = dma_buf_attach(dma_buf, drm_dev->dev);` 后未检查 `IS_ERR(attach)` 就
  `get_dma_buf()` 并写入 `import_attach`：attach 失败时错误指针被存入，dma_buf 引用泄漏。

### 2. GTT 导出映射（`innodpu_gem_prime_map_dma_buf` GTT 分支，约第 851–868 行）

- 现状：对 GTT class 对象逐页 `fh2m_inno_dma_map_page()`（即 `dma_map_page()`）并把结果
  （`inno` 设备时经 `fh2m_cpu_paddr_to_gtt_paddr()` 转换）写入 `sg_dma_address`。
- 现状：`innodpu_gem_prime_unmap_dma_buf`（第 899 行）只 `fh2m_os_sg_free_table` + kfree，
  不执行 `dma_unmap_page()` → 每次跨设备映射/解除循环都会泄漏 DMA 映射（IOVA/DMA 池资源）。
- 逆函数 `fh2m_gtt_paddr_to_cpu_paddr()` 已导出（`INNO_EXT_SYM`），可在 unmap 时还原
  `inno` 分支存储的转换地址。

## 补丁内容（只改 `innosrvkm/innodpu_drm_gem.c`）

1. **import**：先用 `dma_buf->ops == &s_innodpu_gem_prime_dmabuf_ops` 判断是否本驱动导出，
   是才把 priv 当 drm_gem_object 走自导入快速路径；`dma_buf_attach()` 返回
   `IS_ERR(attach)` 时记录错误、`drm_gem_object_release` + kfree 撤销刚创建的私有对象并返回
   NULL，不执行 `get_dma_buf()`。
2. **unmap**：GTT class 时遍历 sg 表，对每个非零 `sg_dma_address` 执行
   `dma_unmap_page(attach->dev, raw, sg->length, dir)`；`attach->dev` 为 `inno` 设备时先用
   `fh2m_gtt_paddr_to_cpu_paddr()` 还原原始 DMA 地址。其他分支（CONTINUOUS_VRAM 等）未调用
   `dma_map_page`，不执行 unmap。

不混入其他改动；不改变 ioctl ABI 或内存布局。

## 已知边界

- 自导入快速路径（同一 innogpu 设备）不经过 GTT 映射分支，桌面 DRI3/PRIME 自导入不受影响；
  本机为单 GPU，**跨设备 GTT 导出路径在本机不可实机触发**，修复以静态正确性 + 回归编译验证为准。
- foreign import 在本机同样无第二设备可实机触发；验证以"自导入路径回归正常"为准。

## 构建开关

`APPLY_FOREIGN_DMABUF_LIFECYCLE_FIX=1`；候选包 `3.3.3.42-patched-27`，构建入口
`scripts/build-patched27-foreign-dmabuf.sh`（固定 SOURCE_DATE_EPOCH）。

## 验证与回退

- 离线验证（已完成）：patched-27 候选包 `debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb`
  （SHA-256 `2586b072901bdc581f16e12d9ab6c8eb2022fb835b6bcd623991af0b42aa2d33`）已通过包边界与
  `6.12.101+deb13-amd64` 离线 DKMS 编译（vermagic 匹配）；不安装、不热切换。
- 实机验证（2026-08-20 已通过）：安装 patched-27 并重启后 `PASS_INSTALL_STATUS` 与
  `PASS_DESKTOP_HWGL` 通过；DRI3/PRIME 自导入路径回归正常（桌面 GL、glamor、AIGLX innogpu、
  DRI3 均正常），确认 import 快速路径的 ops 检查重构未破坏自导入。
- 边界：本机单 GPU，foreign import 与跨设备 GTT export 路径无法在本机实机触发；若后续接入
  第二 DRM 设备或 V4L2 buffer，再补 foreign import 实机验证。
- 回退：直接回退点 `patched-26`。

## 参考

- 内核 `include/linux/dma-buf.h`：`dma_buf_attach()` 返回 ERR_PTR 语义。
- 内核 `drivers/gpu/drm/drm_prime.c`：`drm_gem_prime_import_dev()` 的标准
  ops 检查与 attach 错误处理模式。
- [webkit-dmabuf-investigation.md](../planning/webkit-dmabuf-investigation.md) 静态审计第 3、4 项。
