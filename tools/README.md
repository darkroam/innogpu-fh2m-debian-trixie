# 工具索引

`tools/` 保存构建期确定性变换和最小诊断探针。它们不是用户命令，也不会由 release deb 安装；
调用入口、输入载荷和编译方式由相应脚本或阶段文档负责。新增工具必须在本文件登记，避免出现没有
所有权、验证方式或风险说明的临时程序。

| 工具 | 类型 | 用途与边界 |
| --- | --- | --- |
| `patch-gpupll-object.py` | 构建期对象变换 | 对 Deepin 202504 的 `innogpu.o_shipped` 执行严格单点字节替换；只接受唯一旧序列或已变换状态，其他载荷立即失败 |
| `probe-egl-gbm.c` | 最小 C 探针 | 在指定用户态库环境中创建 GBM device 和 EGL/GLES2 context，报告 backend、renderer 与基本绘制错误，不修改系统配置 |
| `probe-drm-topology.c` | 只读 KMS 探针 | 报告 DRM connector 物理尺寸、encoder、底层 CRTC ID/索引和 active mode，用于核对 WebKit 的 monitor 匹配结果；不 modeset |
| `probe-drm-vblank.c` | 只读 DRM ioctl 探针 | 对指定 CRTC 重复执行带硬超时的相对 vblank wait，记录阻塞时间、序号和内核时间戳；不创建 framebuffer、不 modeset |
| `probe-pdp-invisible-read.c` | 最小 PDP GEM 探针 | 创建单个 invisible GEM；READ 模式测量逐页读取和 munmap，WRITE 模式逐页写入、解除映射并以 READ mapping 验证回写；支持可选 page stride 测量稀疏访问；不提交 GPU 工作、不 modeset |
| `probe-surfaceless-gles2.c` | 最小 C 探针 | 验证 surfaceless EGL/GLES2 初始化和基本绘制，用于区分 Xorg/DDX 与核心 EGL 路径故障 |
| `probe-x11-egl-gles2.c` | 最小 C 探针 | 连接测试 X server 并验证 X11 EGL/GLES2 context，用于隔离 DDX/窗口系统路径 |
| `trace-loader.c` | 诊断 shim | 通过 `LD_PRELOAD` 记录 vendor loader 选择、失败 ioctl，以及 PDP GEM 分配位置和 CPU_PREP/CPU_FINI 的 handle/flags；只用于受控诊断，不得进入发布包 |
| `probe-vulkan-devices.c` | 枚举+执行探针 | **枚举**（默认）：dlopen Vulkan loader，枚举实例版本/扩展、物理设备与队列族，不创建设备；**执行**（`exec [timeout_ms]`）：创建 instance→选 GPU 设备（拒绝 CPU-only）→device/queue→空 command buffer+fence 提交并限时等待→逆序释放。退出码 0=PASS 2=loader 3=无 GPU/初始化 4=device/queue 5=submit/wait；loader 路径可用 `PROBE_VULKAN_LOADER` 覆盖（测试注入）。无需 Vulkan 头文件 |
| `probe-opencl-devices.c` | 枚举+执行探针 | **枚举**（默认）：dlopen OpenCL ICD loader，枚举 platform/device 及关键能力，不创建 context；**执行**（`exec [elements]`）：选 GPU 设备（拒绝 CPU 平台）→context/queue→两 buffer→add kernel 编译运行→阻塞读回→逐元素校验→逆序释放。退出码 0=PASS 2=loader 3=无 GPU 4=context/queue/buffer 5=build 6=run 7=verify；loader 路径可用 `PROBE_OPENCL_LOADER` 覆盖。无需 OpenCL 头文件 |
| `probe-vaapi.c` | 最小 C 探针 | 打开 DRM render 节点并 dlopen libva，枚举 VA-API 驱动、profile 与 entrypoint；不创建 surface/context、不编解码，无需 libva 头文件 |
| `run-vaapi-decode-test.sh` | 真机解码验证 | H.264/HEVC 实际解码：lavfi testsrc2（恰好 30 帧 320x240）→ libx264/libx265 编码 → 软件参考 NV12 framemd5 → **强制 VAAPI 硬解**（hwaccel vaapi + hwaccel_output_format vaapi + hwdownload,format=nv12，无软件回退）→ **真实 framemd5 格式校验**（尾换行、`#dimensions 320x240`、恰好 30 条合法帧记录、NV12 帧大小 115200、32 位 hex hash）→ 逐帧 hash 对比。动态定位 1ec8:9810 render node（`--device` 覆盖并验证 sysfs 身份）；Driver/Firmware 状态门禁：解码前/后两份快照严格解析（OK + 8 类计数字段存在且非负整数）+ 逐字段比较增长（pre 无效解码前即 FAIL）；HUP/INT/TERM 幂等清理后退出 129/130/143；按 `--codec` 校验编码器/解码器/vainfo VLD profile；退出码 0=PASS 1=解码/校验/状态 2=参数/工具/codec 缺失（设备检测前） 3=节点缺失/身份 4=输入/参考 5=超时/清理；fixture 钩子 `FFMPEG_BIN`/`VAINFO_BIN`/`INNOGPU_VAAPI_SKIP_DEVICE_CHECKS`/`FAKE_SYSFS_ROOT`/`INNOGPU_VAAPI_STATUS_FILE` 须显式 `INNOGPU_VAAPI_FIXTURE_MODE=1`，fixture 模式使用**独立命名空间 fixture_***（fixture_tests_total/fixture_vaapi_decode_overall 等），绝不输出任何 `vaapi_decode_*` 权威行。依赖：ffmpeg(+vaapi/libx264/libx265)、vainfo（Debian Trixie 包名 `vainfo`） |
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
