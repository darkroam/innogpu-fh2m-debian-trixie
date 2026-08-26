# 工具索引

`tools/` 保存构建期确定性变换和最小诊断探针。它们不是用户命令，也不会由 release deb 安装；
调用入口、输入载荷和编译方式由相应脚本或阶段文档负责。新增工具必须在本文件登记，避免出现没有
所有权、验证方式或风险说明的临时程序。

| 工具 | 类型 | 用途与边界 |
| --- | --- | --- |
| `patch-gpupll-object.py` | 构建期对象变换 | 对 Deepin 202504 的 `innogpu.o_shipped` 执行严格单点字节替换；只接受唯一旧序列或已变换状态，其他载荷立即失败 |
| `probe-egl-gbm.c` | 最小 C 探针 | 在指定用户态库环境中创建 GBM device 和 EGL/GLES2 context，报告 backend、renderer 与基本绘制错误，不修改系统配置 |
| `probe-drm-topology.c` | 只读 KMS 探针 | 报告 DRM connector 物理尺寸、encoder、底层 CRTC ID/索引和 active mode，用于核对 WebKit 的 monitor 匹配结果；不 modeset。输出契约：active 但内核未提供 mode 名称时 `mode=` 输出稳定占位 `<unnamed>`（如 `mode=<unnamed> refresh=60`），inactive 输出 `mode=-`；空名称绝不产生空字段 |
| `probe-drm-vblank.c` | 只读 DRM ioctl 探针 | 对指定 CRTC 重复执行带硬超时的相对 vblank wait，记录阻塞时间、序号和内核时间戳；失败行附 `errno=%d` 供 inactive-CRTC 守卫机器解析（预期 EINVAL=22）；不创建 framebuffer、不 modeset |
| `probe-dmabuf-self-import.c` | DRM 同设备 PRIME 探针 | 创建 GEM（dumb-buffer 路径）→ `DRM_IOCTL_PRIME_HANDLE_TO_FD` 导出 DMA-BUF → 同一 innogpu 设备 `PRIME_FD_TO_HANDLE` 导入（自导入可返回同 handle，恰好关闭一次）→ 每轮逆序释放 fd/导入 handle/原 handle；导出请求 `DRM_CLOEXEC` 并**严格断言返回 fd 带 FD_CLOEXEC**（缺失即 FAIL，不自补救）；多轮后 `/proc/self/fd` 计数无泄漏。退出码 0=通过 1=ioctl/校验失败 2=参数 3=设备/能力缺失（stdout 输出 `capability=no-dumb-buffer`/`no-prime-export`/`no-prime-import`）。只验证同设备 PRIME self-import，不涉及 foreign import/跨设备 GTT |
| `probe-pdp-invisible-read.c` | 最小 PDP GEM 探针 | 创建单个 invisible GEM；READ 模式测量逐页读取和 munmap，WRITE 模式逐页写入、解除映射并以 READ mapping 验证回写；支持可选 page stride 测量稀疏访问；不提交 GPU 工作、不 modeset |
| `probe-surfaceless-gles2.c` | 最小 C 探针 | 验证 surfaceless EGL/GLES2 初始化和基本绘制，用于区分 Xorg/DDX 与核心 EGL 路径故障 |
| `probe-x11-egl-gles2.c` | 最小 C 探针 | 连接测试 X server 并验证 X11 EGL/GLES2 context，用于隔离 DDX/窗口系统路径 |
| `trace-loader.c` | 诊断 shim | 通过 `LD_PRELOAD` 记录 vendor loader 选择、失败 ioctl，以及 PDP GEM 分配位置和 CPU_PREP/CPU_FINI 的 handle/flags；只用于受控诊断，不得进入发布包 |
| `probe-vulkan-devices.c` | 枚举+执行探针 | **枚举**（默认）：dlopen Vulkan loader，枚举实例版本/扩展、物理设备与队列族，不创建设备；**执行**（`exec [timeout_ms]`）：创建 instance→选 GPU 设备（拒绝 CPU-only）→device/queue→空 command buffer+fence 提交并限时等待→逆序释放。退出码 0=PASS 2=loader 3=无 GPU/初始化 4=device/queue 5=submit/wait；loader 路径可用 `PROBE_VULKAN_LOADER` 覆盖（测试注入）。无需 Vulkan 头文件 |
| `probe-opencl-devices.c` | 枚举+执行探针 | **枚举**（默认）：dlopen OpenCL ICD loader，枚举 platform/device 及关键能力，不创建 context；**执行**（`exec [elements]`）：选 GPU 设备（拒绝 CPU 平台）→context/queue→两 buffer→add kernel 编译运行→阻塞读回→逐元素校验→逆序释放。退出码 0=PASS 2=loader 3=无 GPU 4=context/queue/buffer 5=build 6=run 7=verify；loader 路径可用 `PROBE_OPENCL_LOADER` 覆盖。无需 OpenCL 头文件 |
| `probe-vaapi.c` | 最小 C 探针 | 打开 DRM render 节点并 dlopen libva，枚举 VA-API 驱动、profile 与 entrypoint；不创建 surface/context、不编解码，无需 libva 头文件 |
| `run-dmabuf-regression-test.sh` | DMA-BUF 回归聚合入口 | 编译并运行 self-import + invisible GEM READ/WRITE(+verify) + topology/vblank 探针，Driver/Firmware 双快照严格门禁；参数 `--render-device/--card-device/--size/--iterations/--vblank-samples/--timeout/--read-munmap-limit-ms` 均在设备探测前严格校验（rc=2）；默认动态发现 1ec8:9810 并核对 card/render 同源 BDF；READ munmap 性能门槛默认 max≤40ms（p22 71.9-119.4ms vs 修复后 1.7-2.6ms，可覆盖但必须记录）；vblank 输出全文件严格校验（header/列标题/summary 各恰好一行、样本行按 1..N 顺序、浮点 valid_num + min≤avg≤max、sequence/delta 限制规范 uint32 并按 modulo 2^32 校验回绕、kernel_delta 与相邻 kernel_time 差交叉验证、summary 指标与样本重算交叉验证到 %.3f 容差 0.001ms），inactive CRTC 守卫与 active 共用严格元数据解析并期望快速 EINVAL，且 success=0 时 avg/min/max 必须全零（超时/错误 errno/过慢/重复 header 或列标题/坏浮点/字段乱序/指标与样本不符/kernel_delta 矛盾/uint32 越界均 FAIL，真实回绕合法）；多 CRTC 每个都记录 ok/avg 证据（失败也附完整 per_crtc）；内核日志门禁来源覆盖 innogpu/pvr/drm/gpu/dma_buf/dma_resv/fence，严重事件按词边界+词形匹配（error/fail/fault/bug/hang/timeout/reset/oops/panic/deadlock/stall/corrupt/abort/warn/lockup/wedged 含单复数与进行时），dma_buf timeout、GPU hang、failures、WARNING/WARN_ON、lockup、wedged 均阻断 PASS 而 debug/installed/hangcheck 等 benign 行保持 clean；日志门禁为独立状态机：新严重行 -> fail/FAIL(rc1)；post 不可用或无法证明连续（截断/重排/中间插入/无重叠）-> UNVERIFIED(rc3) 且**一致性失败优先**（不可信窗口不做严重词分类，绝不升级为 fail）；正常环形轮转（after=suffix(before)+new）只检查重叠后新增行（多集差集按 after 原始顺序输出，支持多条新增），重排（after 全为 before 行但乱序）判不连续；内核时间严格单调（%.3f 输入不允许 0.001 倒退容差）；退出码 0=PASS 1=子项/状态 FAIL 2=参数/工具/编译或整体 SKIP 3=设备/能力缺失或整体 UNVERIFIED 5=超时/清理；HUP/INT/TERM 幂等清理退出 129/130/143；fixture 钩子须显式 `INNOGPU_DMABUF_FIXTURE_MODE=1`，输出独立命名空间 `fixture_dmabuf_*`/`fixture_tests_*`，绝不输出权威 `dmabuf_*` 行。能力边界：仅同设备 PRIME self-import；foreign/cross-device/GBM/V4L2/第二 GPU/长期压力/并发保持 UNVERIFIED。依赖：gcc + DRM 头文件（`/usr/include/drm/drm.h`，Debian Trixie 上属 `linux-libc-dev`）、timeout（coreutils）、grep、awk（mawk/gawk） |
| `run-vaapi-decode-test.sh` | 真机解码验证 | H.264/HEVC 实际解码：lavfi testsrc2（恰好 30 帧 320x240）→ libx264/libx265 编码 → 软件参考 NV12 framemd5 → **强制 VAAPI 硬解**（hwaccel vaapi + hwaccel_output_format vaapi + hwdownload,format=nv12，无软件回退）→ **真实 framemd5 格式校验**（尾换行、`#dimensions 320x240`、恰好 30 条合法帧记录、NV12 帧大小 115200、32 位 hex hash）→ 逐帧 hash 对比。动态定位 1ec8:9810 render node（`--device` 覆盖并验证 sysfs 身份）；Driver/Firmware 状态门禁：解码前/后两份快照严格解析（OK + 8 类计数字段存在且非负整数）+ 逐字段比较增长（pre 无效解码前即 FAIL）；HUP/INT/TERM 幂等清理后退出 129/130/143；按 `--codec` 校验编码器/解码器/vainfo VLD profile；退出码设计为 0=PASS 1=解码/校验/状态 2=参数/工具/codec 缺失（设备检测前） 3=节点缺失/身份 4=输入/参考 5=超时/清理。**当前已知实现缺口**：只识别 GNU timeout rc=124，`--kill-after` 产生 rc=137 时会误归普通阶段失败，修复前不得把所有硬超时都解释为 rc=5。fixture 钩子 `FFMPEG_BIN`/`VAINFO_BIN`/`INNOGPU_VAAPI_SKIP_DEVICE_CHECKS`/`FAKE_SYSFS_ROOT`/`INNOGPU_VAAPI_STATUS_FILE` 须显式 `INNOGPU_VAAPI_FIXTURE_MODE=1`，fixture 模式使用**独立命名空间 fixture_***（fixture_tests_total/fixture_vaapi_decode_overall 等），绝不输出任何 `vaapi_decode_*` 权威行。依赖：ffmpeg(+vaapi/libx264/libx265)、vainfo（Debian Trixie 包名 `vainfo`） |
| `generate-binary-manifest.py` | 构建期清单生成 | 从 pinned Deepin deb 确定性生成 `binary-manifest.json`：校验 deb SHA-256，覆盖全部黑盒文件与符号链接，标记 kind/role/license；输出正式 JSON，重复运行结果一致 |
| `validate-binary-manifest.py` | 清单 schema 校验 | 校验 `binary-manifest.json`：source/vendor 路径非空唯一且相对、link_target 不越出载荷根、kind 白名单、文件条目 sha256+size、符号链接条目 link_target；提取工具在写入前调用 |

## 使用约束

1. 探针编译产物和完整输出属于本机临时证据，放在 `/tmp` 或被忽略的 `baselines/` 路径，不进入 Git。
2. `patch-gpupll-object.py` 只由 coherent 构建流程调用；禁止直接修改已安装 `.ko` 后将结果描述为可复现构建。
3. EGL/GBM/X11 探针需要的库路径必须通过对应 `scripts/run-*` 或测试脚本隔离，不能为运行探针全局改写 `/etc/ld.so.conf`。
4. 工具输入、厂商库或内核基线变化后，必须重新验证字节契约、编译依赖和返回码。
5. vblank 探针只允许针对已经活动的 CRTC；失败或返回过快时先保留原始输出，不得自动修改显示配置。
6. KMS 拓扑探针直接使用 `linux-libc-dev` 提供的 DRM UAPI，不依赖 `libdrm-dev`；不得根据 XRandR
   输出编号猜测底层 CRTC 索引。
7. `probe-pdp-invisible-read.c` 的第五个参数是页步长，默认 `1`；只用于测量稀疏 fault 行为，
   不代表驱动预取策略，也不改变设备配置。
