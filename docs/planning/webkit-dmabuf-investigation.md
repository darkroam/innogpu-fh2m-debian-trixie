# WebKit DMA-BUF 与 FH2M 驱动调查

## 状态

本调查于 2026-08-17 开始；以下应用级调查记录以 patched-23 为实验快照，当前设备已升级至
patched-24。独立 DRM/PDP 最小探针确认 FH2M invisible GEM 只读映射的释放路径存在缺陷；单变量
patched-23 已安装并完成一次重启后的驱动、桌面和最小探针验证。Clash
Verge 应用级 CPU A/B 已完成；当前继续研究所有 DMA-BUF 应用共用的 invisible GEM READ 路径。
首个候选只修复 READ mapping 的无意义回写，下一候选只考虑只读预取/批量 `GDDR2SYS`，不混入
同步、vblank 或用户态 ABI 改动。

当前安全 workaround 是只对 WebKit 应用设置：

```sh
WEBKIT_DISABLE_DMABUF_RENDERER=1
```

该变量不改变系统 GLX、Picom 或 innogpu 驱动配置。调查期间继续保留它，故障路径只允许在 CPU
受限、自动超时且不会替换当前桌面的测试进程中启用。

## 已知事实

- 调查实验快照运行 `3.3.3.42-patched-23`；其直接回退点是 `patched-22`，完整图形验收回退点是 `patched-21`。
- FH2M 的 Xorg/GLX、direct rendering、DRI3、Present 和 Picom GLX 已分别通过现有验收。
- Clash Verge 在同一应用和配置下禁用 WebKit DMA-BUF renderer 后，窗口卡顿消失且 CPU 明显下降。
- WebKitGTK 2.52.5 的 `WEBKIT_DISABLE_DMABUF_RENDERER` 位于 GTK UIProcess 的
  `AcceleratedBackingStore` DMA-BUF 路径；该路径还使用 GBM/EGL image、fence 和 DRM vblank。
- Deepin 202504 驱动包含可修改的 DPU GEM/PRIME、`dma_resv`、PVR fence 和 vblank 源码；DRI、GBM、
  EGL/GLVND 用户态是预编译载荷，本仓库没有对应实现源码。
- 最小 GBM/EGL 探针已在 `/dev/dri/card0` 和 `/dev/dri/renderD128` 上通过：GBM backend 为
  `inno`，EGL 1.5、GLES 3.2、renderer `Fantasy II-M`，基本清屏无 GL error。
- DRM CRTC 索引不能从 XRandR 的显示编号直接推断。逐个探测后，底层 CRTC 1 的相对 vblank wait
  连续 10 次成功，序号每次递增 1，稳定周期约 15.7-17.7ms；CRTC 0 和 2 在 300ms 内均不返回，
  CRTC 3 返回 `EINVAL`。
- GPU 的 MSI IRQ 74 正常增长，空闲桌面 2 秒增加 745 次。当前证据不支持“FH2M 整体没有显示
  IRQ”或“所有 CRTC 的 vblank 都失效”。
- 只读 KMS 拓扑显示 HDMI-A-2 是唯一 connected connector，绑定底层 CRTC 1；DRM connector 报告
  `600x330mm`，XRandR/GDK 路径报告 `597x336mm`。WebKit 2.52.5 采用物理尺寸严格相等匹配，默认
  DMA-BUF 运行的 `strace` 中没有任何 `DRM_IOCTL_WAIT_VBLANK`，证明本机当前会自动回退 timer。
- 独立 PDP 探针不依赖 WebKit、GBM 或 EGL，仍可稳定复现 7,646,720 字节 invisible GEM 的高成本
  READ mapping：逐页读取约消耗 92-108ms system CPU，随后的只读 `munmap` 仍消耗约 72-119ms
  system CPU。后者来自驱动逐页执行不必要的 `SYS2GDDR`，已经达到内核修复门槛。

## 已完成结论

- p23 已完成 READ/WRITE、Xorg/GLX、Picom 和基础桌面回归；Clash 启动态 A/B 也已完成。
- p23 只修改仓库中可审查的 DKMS 源码，未修改预编译用户态或 DMA 对象。

## 范围边界

- `innodma.o_shipped` 只有预编译对象，没有可维护的 DMA 描述符实现源码；不尝试修改其内部
  `memcmp`、descriptor 或 completion wait。
- WebKitGTK、GBM、EGL/GLVND 和 Clash Verge 不属于本仓库源码；不在本项目内重写这些组件。
- 只有位于 `patches/`、`scripts/`、`tools/` 或完整 Deepin DKMS 源码中的改动，才进入候选优化。
- 若瓶颈只存在于上述缺失源码中，结论记录为“已定位、当前项目不可修复”，保留应用级 workaround。

## 当前候选链路

### DRM vblank

WebKit UIProcess 的 DMA-BUF accelerated backing store 会创建 DRM vblank monitor。GTK 实现不是固定
选择 CRTC 0：它用 GDK monitor 的 `width_mm`/`height_mm` 筛选 DRM connector，取第一个匹配项的
encoder，再把 encoder 的 CRTC ID 转成 DRM resource 数组索引。多个 connector 物理尺寸相同、尺寸
报告错误或第一个匹配项未活动时，都可能选错 CRTC。

本机底层 CRTC 1 的 vblank 正常，CRTC 0/2 不会唤醒。驱动的 `pdp0_crtc_enable_vblank()` 对实体
CRTC 无条件设置 `vblank_enable=1` 并返回成功，没有拒绝未活动、无有效 mode 或无 IRQ 来源的 CRTC。
因此其他用户态若错误请求未活动 CRTC，可能成功创建 monitor，随后永久阻塞在第一次 relative=1
wait。这是可独立修复的内核接口问题，但不是本机当前 WebKit 卡顿路径：由于 GDK 和 DRM 物理尺寸
不一致，WebKit 没有找到 connector，已经自动使用 timer。不能再把 CRTC 0 超时描述为当前应用根因。

### GEM/PRIME 与隐式同步

WebKit WebProcess/GPUProcess 产生的 DMA-BUF 会在 UIProcess 中导入。需要分别验证：

1. GBM buffer 分配和 PRIME fd 导出；
2. 同设备 PRIME fd 导入；
3. reservation object 中 READ/WRITE fence 的添加和完成；
4. EGL image 导入及释放是否阻塞、失败或产生高频 ioctl；
5. 循环导入/释放是否造成映射、fd、GEM object 或 fence 泄漏。

### 用户态预编译边界

若线程栈停留在 `innogpu_gbm.so`、`innogpu_dri.so` 或同源 EGL/GLVND 库，而对应内核 ioctl 行为正确，
本仓库无法直接源码修复该用户态实现。此时可选方案是：

- 保留 WebKit 级 workaround；
- 取得与 Deepin 202504 ABI 匹配的更新厂商完整用户态后整体升级；
- 在不改动厂商库的情况下，为特定应用选择 WebKit 的 SHM renderer。

禁止单独替换一个 `.so` 或从历史 patched 包拼装用户态。

## 静态审计初步结果

以下结果来自当前安装的 p22 DKMS 源码与 Deepin 202504 基线对照，尚未证明与 WebKit 异常有关：

1. `pvr_buffer_sync.c` 对 Linux 5.19+ 已使用 `DMA_RESV_USAGE_READ/WRITE` 和
   `dma_resv_add_fence()`，GPU 主提交路径不是简单沿用旧 reservation API。
2. `inno_gem_object_cpu_prep_ioctl()` 仍把旧接口的 `bool write` 直接传给 Linux 6.12
   `dma_resv_wait_timeout()`/`dma_resv_test_signaled()`。现代接口第二参数是
   `enum dma_resv_usage`，应通过 `dma_resv_usage_rw(write)` 转换。当前 `false/true` 会分别解释为
   `KERNEL/WRITE`，而不是所需的 `WRITE/READ`。这是可修的 API 兼容问题，但需先确认 WebKit/GBM
   是否调用该私有 CPU_PREP ioctl。受控 `strace` 已确认 DMA-BUF + timer 路径会成对调用私有
   `DRM_PDP_GEM_CPU_PREP`（request nr `0x62`）和 `CPU_FINI`（`0x63`），所以该错误位于实际路径上；
   进一步用诊断 shim 记录到 12 秒内 21 次 CPU_PREP 全部为 `READ`（flags `0x1`）。p22 因此只等
   `DMA_RESV_USAGE_KERNEL` fence，而正确的 `dma_resv_usage_rw(false)` 应等待 `WRITE` fence。现有样本中
   ioctl 均快速返回，尚未证明它是高 CPU 的直接原因。
3. foreign DMA-BUF import 直接把 `dma_buf->priv` 当作 `drm_gem_object` 检查，且未先处理
   `dma_buf_attach()` 的 error pointer；需要最小 foreign-import 测试确认实际影响。
4. GTT export attachment 对每页执行 `dma_map_page()`，当前 unmap 回调只释放 sg table，没有对应
   `dma_unmap_page()`。这可能造成映射生命周期错误，但同一 DRM 设备的 self-import 快速路径可能
   不经过该分支。
5. 实体显示输出使用硬件 IRQ 提交 vblank；hrtimer 只用于 nulldisp。必须先用 ioctl 探针测量当前
   活动 CRTC，不能因源码存在 timer 就推断当前桌面在使用模拟 vblank。
6. 当前 WebKit 路径创建两个大小均为 7,646,720 字节、flags `0x10000000` 的 invisible GEM，随后在
   handle 1/2 上循环执行只读 CPU_PREP。主线程 `/proc/<pid>/syscall` 抽样捕获到长时间停留在
   `munmap(..., 0x74b000)`；`0x74b000` 正是上述 GEM size 向 4KiB 页对齐后的映射长度。
7. invisible VRAM fault 为每个 4KiB 页分配独立系统缓冲并执行 `GDDR2SYS`。VMA close 不检查 CPU_PREP
   的 READ/WRITE 方向，无条件遍历 `vm_head` 并对每页执行 `SYS2GDDR` 后释放。一个 7.3MiB 只读
   surface 因此产生约 1867 次无意义回写。这是当前高 system CPU 和长 `munmap` 的首要根因候选。

## p22 受控 A/B

所有测试均绑定 CPU 0-1，使用相同 Clash Verge 配置、15 秒 `pidstat -t -u -r` 采样和自动超时：

| 组合 | 主进程 CPU | 主进程 system | WebKitWebProcess CPU | 结果 |
| --- | ---: | ---: | ---: | --- |
| `WEBKIT_DISABLE_DMABUF_RENDERER=1` | 5.13% | 1.60% | 27.05% | 正常退出 |
| DMA-BUF + `WEBKIT_FORCE_VBLANK_TIMER=1` | 48.11% | 46.32% | 21.70% | 正常退出 |
| DMA-BUF + 默认选择 | 已完成启动态采样 | 已完成启动态采样 | 已完成启动态采样 | 未发出 WAIT_VBLANK，自动回退 timer；短时空闲样本未复现历史极端忙等 |

timer 组的高开销集中在 UIProcess 主线程，不在 `VBlankMonitor`。15 秒受控 `strace` 显示稳态
CPU_PREP/CPU_FINI 均在微秒到亚毫秒内返回，没有单个慢 ioctl；仍需调用栈或更细粒度采样解释约
46% system CPU。一次受残留 mihomo 污染的默认组曾被强杀，该结果已作废，不得作为 vblank 因果证据。

### invisible 路径定向 A/B

以下两组使用同一个诊断 shim、相同 CPU 0-1 绑定和 15 秒采样：

| 组合 | 主进程 CPU | user | system | 结论 |
| --- | ---: | ---: | ---: | --- |
| p22 原 CPU_PREP 语义 | 42.47% | 1.39% | 41.07% | 高开销主要在内核态 |
| 模拟正确 READ reservation usage | 42.80% | 1.19% | 41.61% | 没有改善，排除 fence usage 为直接根因 |
| 仅对测试进程强制 visible GEM | 91.04% | 90.11% | 0.93% | system 开销消失但变成昂贵的用户态 VRAM 读取 |

visible A/B 证明高 system CPU 属于 invisible staging/回收路径，但直接强制 visible 不是修复。候选应
保留 invisible VRAM 的缓存读 staging，只消除 READ 映射关闭时无意义的 `SYS2GDDR` 回写。

## 独立内核最小复现

`tools/probe-pdp-invisible-read.c` 直接调用 FH2M 私有 PDP ioctl，创建与 WebKit 相同大小的 invisible
GEM，执行 READ CPU_PREP、逐页读取、CPU_FINI 和 `munmap`。它不加载 WebKit、GBM、EGL 或厂商
用户态渲染库。当前 p22 的三轮结果如下：

| 轮次 | touch wall/system | munmap wall/system |
| --- | ---: | ---: |
| 1 | 204.729 / 107.757ms | 157.602 / 71.915ms |
| 2 | 177.730 / 92.476ms | 226.765 / 119.357ms |
| 3 | 189.262 / 94.650ms | 193.688 / 96.716ms |

映射长度为 `0x74b000`，共 1867 页。READ 映射在 `CPU_FINI` 后释放时仍产生与页数成比例的内核态
开销，与源码中 `innodpu_gem_invisible_vram_close()` 无条件逐页调用 `SYS2GDDR` 一致。由此确认：

1. FH2M 驱动确有独立于应用的只读映射回写缺陷；
2. WebKit 的 DMA-BUF 路径会触发该缺陷；
3. 这解释了已测得高 system CPU 中一个可独立量化的重要组成部分；应用端改善幅度以及剩余
   `GDDR2SYS` 成本仍须在修复后做 A/B。

## patched-23 实机验证

2026-08-17 重启后，`scripts/verify-install-status.sh --require-reboot 3.3.3.42-patched-23`
返回 `PASS_INSTALL_STATUS`。DKMS、模块加载、Driver/Firmware、DRM 节点、Xorg、GLX 加速和 DRI3
均通过；桌面硬件 GL 检查返回 `PASS_DESKTOP_HWGL`，Picom、dwm 和 st 正常运行。

同一 7,646,720 字节、1867 页探针在 p23 上的 READ 结果为：

| 轮次 | touch system | munmap system |
| --- | ---: | ---: |
| 1 | 98.392ms | 1.936ms |
| 2 | 92.580ms | 2.589ms |
| 3 | 86.182ms | 1.661ms |

相对于 p22 的 71.915–119.357ms `munmap` system CPU，释放阶段下降约 97–98%。WRITE 三轮每轮
1867 页读回均 `verify=pass`；WRITE `munmap` 保留约 87–129ms system CPU，说明只读优化没有
改变写回语义。

### p23 READ 成本缩放

2026-08-17 在当前 p23、`/dev/dri/renderD128` 上使用同一独立探针，每个大小运行两轮 READ，
只统计 `read_touch` 的 system CPU：

| GEM 大小 | 页数 | system CPU |
| ---: | ---: | ---: |
| 1 MiB | 256 | 16.217–20.489ms |
| 4 MiB | 1024 | 63.942–64.097ms |
| 8 MiB | 2048 | 102.774–134.047ms |
| 16 MiB | 4096 | 259.772–273.163ms |

成本基本按页数线性增长，约为 `0.06–0.07ms/page`。相同探针的 READ `munmap` 只有约
`0.2–7.4ms`，因此 p23 后主要瓶颈已从释放回写转为 page fault 中逐页 staging 分配和
`GDDR2SYS` 搬运。

同日使用探针的 `page_stride` 参数对 16 MiB GEM 做访问模式对照，每组运行两轮 READ：

| 页步长 | 实际触碰页数 | system CPU |
| ---: | ---: | ---: |
| 1 | 4096 | 228.426–324.498ms |
| 2 | 2048 | 107.890–137.653ms |
| 4 | 1024 | 60.645–64.989ms |
| 16 | 256 | 18.630–22.389ms |

结果确认成本随实际 fault 页数增长，顺序访问存在预取收益空间；但稀疏访问不会自动触碰邻页，
因此候选必须限制预取窗口、避免无界内存增长，并提供随机访问回归。

### p23 内核调用栈采样

2026-08-17 使用 `perf record -g --call-graph dwarf` 对 8 MiB、2048 页、stride=1 的单轮 READ
探针采样，捕获 942 个 cycles 样本且无丢样本。调用栈占比显示：

- `innodpu_gem_invisible_vram_vm_fault` 路径占约 `91.84%` 的子调用；
- `fh2m_innodma_memcpy_for_smallbar_sg` 占约 `85.21%`，其中 DMA 描述符准备的 `memcmp` 约
  `25.60%`，DMA 完成等待约 `14.21%`；
- staging 的 `__vmalloc_node` 约 `4.96%`，`vmf_insert_pfn` 约 `1.39%`。

因此已确认热点位于每页 DMA 提交和等待次数。DMA 实现主体位于 Deepin 原包的
`innodma.o_shipped` 预编译对象，本仓库不把其内部优化作为后续任务，保留该证据用于上游或厂商
修复参考。

## 首个修复候选边界

首个候选为 `patch-023` / `3.3.3.42-patched-23`，从当前 p22 顺延且只改变 invisible GEM staging
页的释放行为；历史包不会重建或修改。

1. CPU_PREP 成功时记录本轮 CPU 访问是否包含 WRITE；
2. page fault 创建 staging 页时把访问方向固化到该页记录，不能等到 VMA close 再读取对象状态，
   因为实际调用顺序是 CPU_FINI 后才 `munmap`；
3. READ 页释放 staging 内存但不执行 `SYS2GDDR`；
4. WRITE 页保持原有 `SYS2GDDR` 回写；无法确认有效 CPU_PREP 时保守按 WRITE 处理。

`dma_resv_usage_rw()`、未活动 CRTC vblank 和 foreign DMA-BUF 生命周期问题分别留给后续候选，不得
混入首轮，以保证最小探针和应用 A/B 只有一个内核变量。

## 推进顺序

1. 静态审计 DPU GEM/PRIME、`dma_resv`、PVR fence 和 vblank 实现，记录可疑点但不先改代码。
2. 增加不 modeset 的 vblank 探针，验证当前 p22 的阻塞、序号和刷新周期。（已完成）
3. 增加只读 KMS 拓扑探针，记录 connector 物理尺寸、encoder、CRTC ID、资源索引和 active 状态。
   （已完成）
4. 用短时 `strace` 捕获 WebKit 默认组是否发出 WAIT_VBLANK。（已完成，未发出并自动回退 timer）
5. 跟踪实际 CPU_PREP flags，确认 READ/WRITE 请求被 Linux 6.12 错误转换成哪种 reservation usage。
   （已完成：全部为 READ，当前错误使用 KERNEL，正确值为 WRITE）
6. 仅对受控测试进程用 shim 把 READ 临时变为 READ|WRITE，使 p22 选到等价的 WRITE usage，执行
   同条件 CPU A/B；不得把该诊断方法安装为系统 workaround。（已完成：42.47% 对 42.80%，排除
   reservation usage 错误是高 CPU 的直接根因）
7. 仅对受控测试进程把 WebKit GEM_CREATE 的 invisible flag 清除，验证绕过逐页 staging/copy 路径后
   CPU、`munmap` 和退出行为是否恢复；不得安装为系统 workaround。（已完成：system CPU 降至
   0.93%，但 user CPU 升至 90.11%，证明归属且排除 visible 方案）
8. 增加独立 PDP 探针：创建 7.3MiB invisible GEM，执行 READ CPU_PREP、逐页读取和 munmap，分别记录
   wall/user/system 时间；不依赖 WebKit、GBM 或 EGL。（已完成，独立复现成立）
9. 为只读 CPU mapping 设计“不执行 SYS2GDDR 回写”的最小内核补丁，并保留写映射的原行为；先
   离线编译，不热切换 p22。（patch-023 已实现并完成 p23 实机验证）
10. 扩展最小 GBM 探针，覆盖 DMA-BUF fd、同设备 import 和重复释放。
11. 为只读 page fault 设计受控预取/批量 `GDDR2SYS` 候选；必须保持稀疏访问、WRITE 语义和
    内存上限安全。
12. 为版本大于 22 的新候选分别设计 invisible READ mapping、未活动 CRTC vblank 和
   `dma_resv_usage_rw()` 补丁，不在同一首轮候选中混合变量。

## 验证门槛

- 所有百分比必须保留原始采样、采样周期、CPU 数量和计算方法。
- “busy-loop”需要线程级 CPU 与调用栈；只有进程总 CPU 时只能称为异常 CPU 占用。
- vblank 问题必须由独立 ioctl 探针复现，不能只由 WebKit 卡顿反推。
- PRIME/fence 问题必须由最小 GBM/DRM 测试复现，不能只由 EGL 或页面动画失败反推。
- 修改内核后必须重新执行 p21/p22 已有的 PVR、DRM/fbdev、Xorg/GLX、真实 VT、显示和 Picom 回归。
- 修复后的独立探针中，READ touch 可以保留现有 `GDDR2SYS` 成本，但 READ `munmap` 的 system CPU
  应显著下降；WRITE 模式必须证明写入在解除映射后仍可读回。
- 预取/批量候选必须提供随机页、顺序页、重复映射、内存峰值和 WRITE 回归数据；完成前不得安装
  或重启。

## DMA-BUF 回归入口（2026-08-24，~/7.md）

- `tools/run-dmabuf-regression-test.sh` 聚合：设备动态发现（1ec8:9810 render/card 同源 BDF）→
  PRIME 同设备 self-import（新探针 `tools/probe-dmabuf-self-import.c`，CREATE_DUMB→HANDLE_TO_FD→
  FD_TO_HANDLE→逆序释放，CLOEXEC 验证，多轮 fd 无泄漏）→ invisible GEM READ/WRITE+verify（READ
  munmap 性能门槛默认 max≤40ms system CPU，依据本文 p22 基线 71.9-119.4ms 与修复后 1.7-2.6ms）→
  topology 动态取 active/inactive CRTC 资源索引（不按 XRandR 输出名猜索引）→ active vblank 相对 wait
  ≥10 样本全成功无 fast return/nonadvancing → inactive CRTC 守卫（快速 EINVAL=22，timeout/错误 errno/
  过慢 FAIL，无 inactive 诚实 SKIP）→ Driver/Firmware 双快照严格门禁。
- **能力边界（不冒充）**：self-import 只证明同设备 PRIME 快路径；foreign import（其他 exporter）、
  跨设备 GTT export、V4L2/第二 GPU、长期压力与并发流仍 UNVERIFIED；vblank 是独立同步子项，不是
  DMA-BUF export/import 证据；invisible GEM READ/WRITE 是 PDP 私有语义，不是 DMA-BUF 生命周期测试。
- fixture 测试 137 项（tests/unit/run-dmabuf-regression-tests.sh），CI 无 /dev/dri 可跑；真机证据待
  监督授权后采集，`runtime_dmabuf_regression` 当前保持 UNVERIFIED。

## 风险与回退

- 静态审计和只读 ioctl 探针无需重启，失败时直接停止测试进程。
- WebKit 故障复现可能耗尽 CPU，必须限制 CPU 并设置自动超时；异常时恢复
  `WEBKIT_DISABLE_DMABUF_RENDERER=1` 即可，不回退驱动。
- p23 已完成安装、一次重启和驱动/桌面/最小探针验证；当前不再重复重启。继续保留 p22 直接回退包
  和 p21 完整图形回退点，等待 Clash Verge 应用级 A/B 后再判断是否需要后续候选。
