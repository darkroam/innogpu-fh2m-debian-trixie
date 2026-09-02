# 测试体系策略（重构，4.0.0-i1 基线）

> 2026-08-21 建立，2026-08-31 完成计数权威收敛。目标：后续优化遵循"先写失败测试 → 修改实现 →
> 回归验证"。本策略定义分层、能力域、输出规范、风险与执行顺序。约束：不安装驱动、不切换模块、
> 不重启；runtime 域仅实机授权后执行。

## 一、现有测试盘点（CONFIRMED，2026-09-02 核对）

### 当前 CI/沙箱套件汇总（2026-09-02）

数量以各 runner 的运行时汇总行为机械事实；本表是文档中唯一的当前计数汇总。

| 范围 | 入口与用例 | 小计 |
| --- | --- | --- |
| unit | manifest 9 + license 50 + version 7 + extractor 7 + results parser 19 + exec probes 12 + VA-API 56 + DMA-BUF 147 + DRI repair 34 + collab 26 + suspend/resume 12 | 11 个入口 / 379 项 |
| 其他 CI/沙箱 | fbterm 1 + package 9 + Picom install 3 + Picom session 3 + xdisplay 5 | 5 个入口 / 21 项 |
| **合计** | 不含需真实设备/root/副作用授权的 runtime 项 | **16 个入口 / 400 项** |

| 测试 | 位置 | 断言 | 权限/环境 | 修改系统 |
| --- | --- | --- | --- | --- |
| fbterm 静态 | tests/fbterm/run-static-tests.sh | bash -n + 补丁内容 grep（set -e 生效） | 无 | 否 |
| Picom 安装 | tests/picom/run-install-tests.sh | 空 HOME 配置/幂等/既有配置保留 | 无（fake HOME） | 否 |
| Picom 会话 | tests/picom/run-session-tests.sh | 优先/回退/单实例 | 无（fake 命令） | 否 |
| xdisplay 安装边界 | tests/xdisplay/run-install-tests.sh | 拒绝私有副本/钩子/幂等/watcher/xprofile | 无（fake HOME） | 否 |
| 包边界 | tests/package/run-boundary-tests.sh | fixture 覆盖新版本通过/私有载荷拒绝/p20 复用拒绝/helper 一致/固件完整/非 amd64/Installed-Size | dpkg-deb | 否 |
| manifest/版本/提取器 | tests/unit/run-{manifest,version,extractor}-tests.sh | schema、license、路径、哈希、恢复与版本排序 | shell/python/dpkg | 否 |
| 许可证审计 | tests/unit/run-license-audit-tests.sh | 三层模型正反例：根 GPLv3 越界覆盖 drivers/ 拒绝、上游 MIT notice 缺失拒绝、project-tools/driver-source 允许清单混入 confidential/vendor/deb/**patches**/无许可路径拒绝（含 `^patches/`、`^debs/` 整目录排除）、**非 drivers 失败关闭分类**（`original_roots` + 显式映射；未知路径如 external/foo.c 拒绝、LICENSES/ 标准文本未映射拒绝——无全局默认 GPL）、**路径绑定 NOTICE 门禁**（新第三方路径缺 notice_gate 条目拒绝、条目标记缺失如 Yuxuan Shui 拒绝）、`vendor-binary`/`NOASSERTION` 当许可证拒绝、畸形 SPDX 拒绝、脏树发布拒绝、allowlist 路径穿越/重复/symlink 拒绝、归档内容不属于 HEAD 拒绝；正例：project-tools 从干净提交构建成功、上游 MIT 随 notice 发布、picom 补丁（文件级 MPL-2.0，固定 commit）与 fbterm 补丁（(C) 2008 dragchan，GPL-2.0-only）材料封存后通过、driver-source 仅含明确许可文件通过、无本地 deb/vendor 时审计仍可运行、只读 `.git` 环境审计通过；外加确定性/陈旧清单/条款缺失/权限模式保留/`--draft` 结构测试 | python/git，临时 fixture repo | 否 |
| runtime 结果解析 | tests/unit/run-results-parser-tests.sh | 严格解析、授权、重复/粘连/缺失输入、`#` 注释跳过 | 无设备，隔离 baseline | 否 |
| Vulkan/OpenCL 探针失败路径 | tests/unit/run-exec-probes-tests.sh | 编译、loader/设备失败、格式与清理 | gcc，无设备可跑 | 否 |
| VA-API 解码控制流 | tests/unit/run-vaapi-decode-tests.sh | 参数/工具/设备/身份/输入/参考/硬解/超时（rc=124 与 rc=137 忽略 TERM→SIGKILL 忙循环 fixture 均归类超时，退出码 5）/真实 framemd5 格式负例/聚合/硬解参数断言/状态门禁严格解析/mktemp/TERM 清理/无残留；fixture 模式独立命名空间 fixture_*，不产出权威 PASS | 无设备（fake ffmpeg/vainfo/sysfs/status），隔离 baseline | 否 |
| DRI repair 服务生命周期 | tests/unit/run-dri-repair-tests.sh | helper 三态判定（absent/owned/foreign；外国普通文件/符号链接拒绝覆盖）与 unit `ExecStart` 一致（包 `/usr/sbin` vs 源码 fallback `/usr/local/sbin` 区分）、**PATH 注入反例（任意同名程序不得被持久化）**、失败回滚**只删本次新建**（已有有效安装在重装失败后保留）、`enable/start` 失败传播、幂等安装/卸载、**package-absent 只清 DRI 自有路径**（不触碰 userspace/modules-load）、**版本不匹配零副作用**、**精确所有权**（只删除规范化目标等于本仓库 `scripts/repair-dri-nodes.sh` 的符号链接；同名外国仓库链接/普通文件保留）、**测试根安全（空//相对路径 fail closed）**、包 helper 分支正例、无硬编码目标用户名/无 root `$HOME` 回退静态反例 | 无 root/systemd//dev（fake systemctl + 测试根前缀钩子，默认关闭） | 否 |
| DMA-BUF 回归聚合 | tests/unit/run-dmabuf-regression-tests.sh | 参数/设备发现与身份/self-import（含 create_size 与 CLOEXEC 严格断言）/READ 逐轮唯一性解析/性能门槛/WRITE verify/topology 多 CRTC/vblank（active 逐样本校验：顺序 + delta/kernel_delta 数值自洽 + uint32 回绕 + summary 指标与样本重算交叉验证 + 每 CRTC 独立证据；inactive 全 CRTC 守卫：success=0 时指标全零）/状态门禁（增减均拒）/内核日志门禁（独立状态机：新严重行 FAIL/rc1；post 不可用/截断/重排/插入/无重叠 UNVERIFIED/rc3；正常环形轮转只查重叠后新增行）/mktemp/超时/TERM 清理/汇总 + 真实 C 探针契约测试（含生产构建 fixture 钩子编译剔除门禁与正常路径 fd_leak=unknown）；fixture 模式独立命名空间 fixture_dmabuf_*，零权威 dmabuf_* 行 | 无设备（fake sysfs/dev/探针 + 真实探针 FIFO 路径），隔离 baseline | 否 |
| collab 结构 | tests/unit/run-collab-structure-tests.sh | 目录命名/编号唯一/request+report 模板齐全/INDEX 与目录按编号精确双向一一对应（R01 不误配 R010、重复行、孤立行、孤立目录、日期与主题一致）/状态白名单/根目录散放文件/根目录或内部符号链接/嵌套目录/仅 Markdown/INDEX 与隐藏 Markdown 的大小写无关隐私扫描（两侧共用 tools/private-data-patterns.txt）/缺 INDEX/collab 目录缺失视为通过 | python3，无设备 | 否 |
| suspend/resume 静态 | tests/unit/run-suspend-resume-tests.sh | patch-024 dry-run/应用、OFF 门禁位于 OPP/PreClock 前、单文件范围、`4.0.1-i1`/epoch 失败关闭、编译树与包源码双应用、patched-28 legacy 接线 | shell/python/patch，复制跟踪源码到 `/tmp` | 否 |
| runtime 能力基线 | tests/runtime/run-capability-baseline.sh | 12 能力域、35 项；默认只读，人工结果显式合并 | 沙箱/真机授权 | 授权项可能有副作用 |

另：`scripts/check-docs.sh`（静态，链接/登记/隐私/版本/边界）、`check-source-parity.sh`（只读 parity）、
`compare-oracle-candidates.sh` + `compare-module-symbols.sh`（integration oracle）、
`check-deb-dkms-build.sh`（integration 离线编译，需本机内核头）。

**盘点结论**：unit、fixture、static、integration 和 runtime 五层均已有入口；当前规模见顶部唯一
套件汇总。主要缺口是完整 DKMS integration 依赖本机 headers，以及 runtime 能力仍缺真机证据。

## 二、分层定义与映射

| 层 | 定义 | 现有 | 缺口 |
| --- | --- | --- | --- |
| unit | manifest/许可证审计/版本排序/路径/哈希/配置解析/执行探针/VA-API 解码/DMA-BUF 回归/DRI repair/collab 结构的纯函数用例 | 见顶部套件汇总 | 构建器 headers/helper 失败 fixture 待补；法律授权仍需人工审查 |
| fixture | 恶意路径/缺失/坏哈希/损坏链接/重复项 | tests/fixtures/ + 脚本隔离构造 | 覆盖随新输入边界持续扩展 |
| static | shell 语法/脚本登记/文档链接/隐私/构建器输入 | check-docs.sh、fbterm 静态 | 语义与法律授权仍需人工审查 |
| integration | staging/DKMS 离线/包边界/可复现/oracle | 包边界、oracle、parity、离线编译 | 可复现双构建入 CI 需内核头（本机可跑） |
| runtime | 真机 DRM/fbdev/Xorg/GL/音频/Picom/显示/回退 | 35 项能力基线 + Phase 4 A1-A12 | 4 项仍为 UNVERIFIED |

## 三、显卡标准能力域（12 项，枚举 vs 实际执行必须分开）

| # | 能力域 | 枚举测试（离线/只读可跑） | 实际执行（需真机+授权） | 当前结论 |
| --- | --- | --- | --- | --- |
| 1 | PCI/内核驱动 | `lspci`、dkms status、modinfo vermagic | dmesg PVR/固件/错误计数 | OBSERVED PASS（Phase 4） |
| 2 | DRM/KMS | `ls /dev/dri`、drm_info、sysfs | modeset/热插拔/分辨率切换 | 节点/拓扑 PASS；modeset 矩阵 UNVERIFIED |
| 3 | fbdev/控制台 | `ls /dev/fb0`、fb ioctl | 真实 VT fbterm 绘制/清屏/重入 | OBSERVED PASS（历史 VT） |
| 4 | EGL/GBM/DRI | tools/probe-egl-gbm、probe-x11-egl-gles2 | buffer 分配/DMA-BUF 导入导出 | EGL/X11 PASS；GBM 专项 UNVERIFIED；DMA-BUF 同设备 PRIME self-import PASS（跨设备/GBM 导入 UNVERIFIED） |
| 5 | OpenGL/GLX/GLES | `glxinfo`/check-desktop-hwgl | 最小 GL 程序（非 llvmpipe） | OBSERVED PASS（4.3 core/ES 3.2） |
| 6 | Vulkan | tools/probe-vulkan-devices | instance/device/queue + command submit/fence wait | 枚举及最小执行 PASS；实际渲染未覆盖 |
| 7 | OpenCL/计算 | tools/probe-opencl-devices | 最小 kernel/buffer 读写与逐元素校验 | 枚举及最小执行 PASS |
| 8 | 视频 | tools/probe-vaapi / vainfo / tools/run-vaapi-decode-test.sh | 固定 H264/HEVC 样本解码与输出校验 | profile 枚举 PASS；H.264 Main+HEVC Main 实际解码 PASS；无 VA encode entrypoint |
| 9 | DMA-BUF/同步 | 静态审计（patch-023/025/027）+ run-dmabuf-regression-test.sh | DRI3、同设备 PRIME self-import、fence、失败路径 | Phase 4 DRI3/PRIME 基础回归 PASS；专项同设备 PRIME self-import runtime 回归 PASS（2026-08-26 真机）；foreign/跨设备/V4L2 仍 UNVERIFIED |
| 10 | 显示输出 | xrandr/DRM 拓扑交叉核对 | 内置屏/外接/插拔/合盖恢复 | 当前 HDMI 拓扑 PASS；切换/热插拔/合盖 UNVERIFIED |
| 11 | 桌面合成/应用 | Picom 进程/配置枚举 | backend 确认、透明/圆角/拖拽/WebKit DMA-BUF | 进程/配置 PASS；实际 GLX backend UNVERIFIED |
| 12 | 音频/显示音频 | aplay -l、wpctl status | 实际播放 HDA + FH2M HDMI 并确认听感 | 枚举/默认 sink PASS；受控听感 UNVERIFIED |

**规则**：枚举成功 ≠ 渲染成功；能力不存在/工具缺失/无显示器/无真 VT → 只允许 `SKIP`/`UNVERIFIED`，
不得伪造 PASS。

## 四、标准输出基线表（`tests/runtime/` 输出规范）

| 能力 | 工具/接口 | 必须记录的标准输出 | 实际使用验证 | 当前结论 |
| --- | --- | --- | --- | --- |
| DRM/KMS | drm_info、sysfs、/dev/dri | card/render、CRTC、connector、mode、plane | modeset/显示切换 | 枚举 PASS / modeset UNVERIFIED |
| fbdev | /dev/fb0、fb ioctl、真 VT | 节点、分辨率、mmap/ioctl | fbterm 绘制和重入 | PASS |
| GL/GLX/GLES | glxinfo、eglinfo | renderer、vendor、direct、accelerated、版本 | 最小 GL 程序 | PASS |
| EGL/GBM/DRI | eglinfo、GBM/DRI 探针 | vendor、extensions、device、DMA-BUF | buffer 分配/导入 | EGL/X11 PASS / GBM 专项 UNVERIFIED |
| Vulkan | vulkaninfo、exec 探针 | device、API、queue、submit、fence wait | command submit/fence wait | PASS（实际渲染未覆盖） |
| OpenCL | clinfo、exec 探针 | platform、device、kernel、读回校验 | 最小 kernel/buffer | PASS |
| VA-API | vainfo、tools/run-vaapi-decode-test.sh | vendor、profile、entrypoint、30 帧 NV12 framemd5 hash | 固定 H.264/HEVC Main 样本强制硬解 + 输出校验 | 解码 PASS（H264 Main+HEVC Main 30 帧 320x240 NV12 校验）/ 无 encode entrypoint |
| 音频 | aplay、wpctl | ALSA card、PCM、PipeWire sink | 实际播放并确认听感 | 枚举 PASS / 听感 UNVERIFIED |

## 五、结果格式与退出码约定（统一，全部测试已实现）

每条用例输出一行机器可读结果（2026-08-21 全部 tests/ 脚本已按此格式改造）：

```text
<suite>_tNN=PASS
<suite>_tNN=FAIL reason=<原因>
<suite>_tNN=SKIP reason=<原因>
```

- 汇总行：`tests_total=<N> tests_passed=<P> tests_failed=<F> tests_skipped=<S>`。
- PASS → 退出码 0；FAIL → 1；SKIP → 2（或由总控显式区分，不得转 PASS）。
- 有副作用测试必须显式参数确认；临时文件必须 `mktemp` + `trap` 清理。
- 离线/沙箱结果与真机结果**分开保存**（`baselines/` 紧凑标记 + 版本化审计日志）。

当前 CI/沙箱规模见本文顶部唯一套件汇总，本节不复制易过时数量。

## 六、覆盖清单（本策略要求逐项落实）

**fixture 至少覆盖**：缺少 source deb、源包哈希错误、manifest 缺字段、重复路径、绝对路径、
`../` 穿越、非法 kind/link_target、缺失/错误 vendor 文件、中断恢复、`--check-only` 缺失文件必须失败。
→ **已全部落地测试**：`tests/unit/run-manifest-tests.sh`（恶意清单 fixtures）与
`tests/unit/run-extractor-tests.sh`（vendor 缺失时 --check-only 失败、完整提取、幂等重跑、
提取后 --check-only 通过、哈希篡改 --check-only 失败、中断/残留重建、源 deb SHA 不匹配失败；
使用临时 fixture deb 与隔离 vendor 树，通过提取器新增的 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖）。

**构建测试至少覆盖**：headers 缺失、`SOURCE_DATE_EPOCH` 缺失、版本低于 patched-27、helper 缺失、
`.o.cmd` 进入包、双构建哈希一致、oracle 文件清单/载荷/DKMS 源码/模块符号一致。
→ 大部分由构建器门禁 + oracle 脚本覆盖（CONFIRMED）；**补"helper 缺失"与"headers 缺失"的
失败用例**（fixture，不实际构建）。

**文档/许可证测试至少覆盖**：链接存在、脚本登记完整、README 当前版本准确、runtime 权威统计一致、
无个人路径/token/serverauth、历史版本不冒充当前、Markdown 表格结构、许可证逐路径分类/manifest
证据语义和 Phase 5 边界。→ `check-docs.sh` 调用 `audit-licenses.py` 覆盖可机械验证部分；
许可证 fixture 覆盖漂移负例；法律授权和文字语义仍需人工审查。

## 七、每测试声明模板（tests/README.md 矩阵登记）

每个测试记录：前置条件、只读性、root 需求、真实设备需求、重启需求、失败恢复方式、
可运行环境（SSH/沙箱/容器/真机）。

## 八、执行顺序

1. `scripts/check-docs.sh`（静态门禁，任何变更后必跑）
2. `tests/fbterm`、`tests/picom`、`tests/xdisplay`（fixture/static，CI 可跑）
3. `tests/package`（fixture，CI 可跑）
4. `tests/unit`（manifest 恶意输入/版本排序/提取器隔离，CI 可跑）
5. integration（本机）：parity/oracle/离线 DKMS（需内核头）
6. runtime：`tests/runtime/run-capability-baseline.sh`（2026-08-24 已实现；只读默认，设备项
   沙箱输出 SKIP/UNVERIFIED；`--allow-authorized-tests` 仅解锁人工命令清单，`--results-file`
   合并人工执行结果；有副作用操作（VT/modeset/播放/GPU 执行）永不自动运行）

## 九、runtime 实际覆盖（2026-08-24）

- 35 项：静态/探测类 15 PASS（PCI/包版本/DKMS/vermagic/模块/参数/proc 状态/固件/错误计数/
  dma_resv 源码存在性等），设备类 19 SKIP（无 /dev/dri 或人工执行项），1 UNVERIFIED（沙箱 GL
  为 llvmpipe）。
- 命名约定：`dmabuf_source_fix_present` 明确为**源码存在性**检查，不是 DMA-BUF 运行能力；
  真实 DMA-BUF 回归已由 2026-08-26 真机运行升级为 PASS（同设备 PRIME self-import，证据封存）；跨设备/GBM/V4L2/
  长期压力/并发路径仍 UNVERIFIED。
- 工具缺失（vulkaninfo/clinfo/vainfo/glxinfo/drm_info/xrandr/wpctl）→ SKIP reason=tool_missing；
  工具存在但无 DRI 节点初始化失败 → SKIP；枚举失败不笼统隐藏。
- 真机证据通过 `--results-file` 合并；环境探测元数据和人工结果来源分别记录，不能视为同一次会话。
- `--results-file` 严格解析（2026-08-24）：接受全部已定义项（含 egl_x11_probe/gl_execution）、
  未知名/状态告警忽略、重复名取最后、粘连行拒绝、PASS/FAIL 必须带证据 reason、缺失文件 rc=2、
  强制 `--allow-authorized-tests`；fixture 测试见 tests/unit/run-results-parser-tests.sh（含 `#`
  注释行跳过与不泄漏断言）。
- 真机证据合并（2026-08-24）：fbterm、EGL/X11、桌面 GL、Vulkan execution、OpenCL execution 和
  VA-API 实际解码为 PASS，DMA-BUF 回归 2026-08-26 真机 PASS；权威汇总 22 PASS / 9 SKIP / 4 UNVERIFIED，overall=UNVERIFIED。
- Vulkan/OpenCL 最小执行（2026-08-24）：探针新增 `exec` 模式（dlopen、无头文件），
  execution 判定标准 = 真正创建资源 + 执行最小操作 + 校验结果：
  - Vulkan：instance→GPU device（拒绝 CPU-only）→queue→空 cmd buffer+fence 提交→限时等待→释放；
  - OpenCL：GPU device→context/queue→buffer→add kernel 编译运行→阻塞读回→逐元素校验→释放；
  - 退出码分级（2 loader/3 无 GPU/4-7 各阶段失败），机器可读输出；loader 路径 env 注入；
  - 枚举成功 ≠ execution PASS；无设备时探针返回可解释失败（rc=2/3）；
  - 真机 execution 证据（2026-08-24 用户实测已 PASS）：vulkan exec 5000（queue+fence submit+wait）
    与 opencl exec 1024（add kernel+读回逐元素校验）均在 Fantasy II-M 上执行并验证；
    runtime_vulkan_execution/opencl_execution=PASS（evidence: baselines/runtime-results-20260824.txt）。
  - 测试：tests/unit/run-exec-probes-tests.sh（CI 无 /dev/dri 可跑）覆盖编译/缺 loader/
    无设备/枚举回归/超时清理/机器格式。
- VA-API H.264/HEVC 实际解码（2026-08-24）：`tools/run-vaapi-decode-test.sh --codec h264|hevc|all`
  判定链 = 输入生成（lavfi testsrc2 恰好 30 帧 320x240→libx264/libx265）→ 软件参考（NV12 framemd5）→
  **强制 VAAPI 硬解**（hwaccel vaapi + hwaccel_output_format vaapi + hwdownload,format=nv12，初始化失败即
  FAIL，无软件回退）→ **真实 FFmpeg framemd5 格式校验**（尾换行、`#dimensions 320x240`、恰好 30 条合法帧
  记录 stream,dts,pts,duration,size,hash、NV12 帧大小 115200、32 位 hex hash）→ 逐帧 hash 对比；
  render node 动态定位 1ec8:9810 并验证 sysfs 身份（`--device` 覆盖）；退出码设计为 0-5，输入生成、
  软件参考和硬解三阶段均把 timeout rc=124/137 归类为 rc=5；Driver/Firmware 状态
  门禁：**两份完整快照**（解码前/后）严格整行解析——Driver/Firmware 行必须 `^...OK[[:space:]]*$`（拒绝
  OKAY 等前缀冒充）、8 类计数字段整行 `^字段名:[[:space:]]+[0-9]+[[:space:]]*$`（拒绝多余 token）且各恰好
  出现一次（拒绝重复行）、值在 awk 中输出规范十进制（剥离前导零，避免 Bash 八进制解析 08/09 误判）——
  并按字段逐项比较增长（pre 快照无效在解码前即 FAIL，独立 gate 不计入 codec 计数）；HUP/INT/
  TERM 幂等清理后退出 129/130/143，mktemp 失败 rc=2；按 `--codec` 校验 libx264/libx265 编码器、h264/hevc
  解码器与 vainfo VLD profile；fixture 钩子须显式 `INNOGPU_VAAPI_FIXTURE_MODE=1`，fixture 模式使用**独立
  命名空间 fixture_***（fixture_vaapi_decode_h264=...、fixture_tests_total=...、fixture_vaapi_decode_overall=...），
  绝不输出任何 `vaapi_decode_*` 权威行，reason 仍附 -mode=fixture；`runtime_vaapi_decode` 仅当 H.264 与
  HEVC 均完成真实硬解+输出校验后升级 PASS——2026-08-24 真机执行（监督者于 c7b3a40 上沙箱外运行
  `bash tools/run-vaapi-decode-test.sh --codec all`）：H264 Main 与 HEVC Main 均强制 VA-API 硬解、各 30 帧
  320x240 NV12 framemd5 hash 与软件参考一致、Driver/Firmware 状态门禁通过 → 升级 PASS
  （evidence: baselines/runtime-results-20260824.txt）；**能力边界**：仅 Main/Main 8-bit 4:2:0，H.264 High/
  Constrained Baseline、HEVC Main10、编码、播放/长时/并发/4K/性能功耗均未验证；枚举
  profile/entrypoint 或 ffmpeg 退出 0 不等于实际解码成功；测试：tests/unit/run-vaapi-decode-tests.sh
  （fake fixture，CI 无 /dev/dri）。
- DMA-BUF 回归（2026-08-24 实现，2026-08-26 真机验证）：`tools/run-dmabuf-regression-test.sh` 聚合入口，编译并运行
  四探针，判定链 = 设备发现/身份（动态 1ec8:9810 render+card 同源 BDF）→ 同设备 PRIME self-import
  （CREATE_DUMB→HANDLE_TO_FD→FD_TO_HANDLE→逆序释放，CLOEXEC 验证，多轮无 fd 泄漏）→ invisible GEM
  READ（严格输出解析 + READ munmap 性能门槛：max(system_ms) ≤ 默认 40ms，区分 p22 71.9-119.4ms 与
  修复后 1.7-2.6ms，可覆盖但记录）→ WRITE+readback（每轮 verify=pass）→ topology 动态取 active/inactive
  CRTC 资源索引（非 XRandR 猜索引；active 且 mode 名称为空时探针输出 `mode=<unnamed>` 占位，空名称不产生空字段）→
  active vblank（≥10 相对 wait、全成功、序号推进、无 fast
  return/nonadvancing）→ inactive vblank 守卫（快速 EINVAL=22 通过，timeout/错误 errno/过慢 FAIL，无
  inactive 时诚实 SKIP）→ Driver/Firmware 双快照严格门禁（8 字段逐项比较）；退出码 0=PASS 1=子项/状态
  FAIL 2=参数/工具/编译或整体 SKIP 3=设备/能力缺失或整体 UNVERIFIED 5=超时/清理；fixture 模式独立命名
  空间 fixture_dmabuf_*/fixture_tests_*，零权威 dmabuf_* 行；`runtime_dmabuf_regression` 已由 2026-08-26
  真机运行升级为 **PASS**（同设备 PRIME self-import/READ/WRITE/active vblank/状态门禁全 PASS，inactive guard 按
  拓扑通过，kernel_log=clean；证据见 `baselines/runtime-results-20260824.txt`）；**能力边界不变**：仅同设备
  PRIME self-import，foreign import/跨设备 GTT export/GBM/V4L2/第二 GPU/长期压力/并发仍 UNVERIFIED；探针：
  probe-dmabuf-self-import.c（新）、probe-pdp-invisible-read.c、
  probe-drm-topology.c、probe-drm-vblank.c（失败行加 errno）；测试：tests/unit/run-dmabuf-regression-tests.sh
  （fake fixture，CI 无 /dev/dri）。

## 十、风险与未覆盖

- 完整 DKMS 构建需本机内核头 → CI 无法跑 integration 编译（标记 SKIP 而非 PASS）。
- VA-API 实际解码仅覆盖 H.264 Main/HEVC Main（30 帧 320x240 NV12 输出校验）；H.264 High/
  Constrained Baseline、HEVC Main10、VA-API 编码、DMA-BUF 回归（同设备 PRIME self-import 真机 PASS，但
  foreign/cross-device/GBM/V4L2/长期压力/并发路径不可验证）、modeset/热插拔/合盖、Picom GLX backend、音频听感及多硬件
  矩阵仍为 UNVERIFIED；枚举或命令退出成功不能替代对应行为证据。
- runtime 脚本已落地；新增有副作用的真机项仍需监督授权，并通过结果文件合并审计证据。

## 证据索引

`tests/README.md`、`scripts/check-docs.sh`、`docs/planning/capability-survey.md`、
`docs/planning/phase4-device-validation.md`。
