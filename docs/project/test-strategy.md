# 测试体系策略（重构，4.0.0-i1 基线）

> 按 `~/5.md` 第二阶段产出（2026-08-21）。目标：后续优化遵循"先写失败测试 → 修改实现 →
> 回归验证"。本策略定义分层、能力域、输出规范、风险与执行顺序。约束：不安装驱动、不切换模块、
> 不重启；runtime 域仅实机授权后执行。

## 一、现有测试盘点（CONFIRMED，2026-08-21 核对）

| 测试 | 位置 | 断言 | 权限/环境 | 修改系统 |
| --- | --- | --- | --- | --- |
| fbterm 静态 | tests/fbterm/run-static-tests.sh | bash -n + 补丁内容 grep（set -e 生效） | 无 | 否 |
| Picom 安装 | tests/picom/run-install-tests.sh | 3 项（空 HOME 配置/幂等/既有配置保留） | 无（fake HOME） | 否 |
| Picom 会话 | tests/picom/run-session-tests.sh | 3 项（优先/回退/单实例） | 无（fake 命令） | 否 |
| xdisplay 安装边界 | tests/xdisplay/run-install-tests.sh | 5 项（拒绝私有副本/钩子/幂等/watcher/xprofile） | 无（fake HOME） | 否 |
| 包边界 | tests/package/run-boundary-tests.sh | 7 项 fixture（新版本通过/私有载荷拒绝/p20 复用拒绝/helper 一致/固件完整/非 amd64/Installed-Size） | dpkg-deb | 否 |

另：`scripts/check-docs.sh`（静态，链接/登记/隐私/版本/边界）、`check-source-parity.sh`（只读 parity）、
`compare-oracle-candidates.sh` + `compare-module-symbols.sh`（integration oracle）、
`check-deb-dkms-build.sh`（integration 离线编译，需本机内核头）。

**盘点结论**：现有测试都是 static/fixture/integration 且只读；**缺少 unit 层**（manifest/版本排序/
路径校验的独立用例）、**缺少能力域 runtime 基线脚本化**、fixture 目录未集中（各脚本内联构造）。

## 二、分层定义与映射

| 层 | 定义 | 现有 | 缺口 |
| --- | --- | --- | --- |
| unit | manifest/版本排序/路径/哈希/配置解析的纯函数用例 | — | **新增** `tests/unit/run-manifest-tests.sh` 等 |
| fixture | 恶意路径/缺失/坏哈希/损坏链接/重复项 | 包边界内联构造 | **新增** `tests/fixtures/` 集中 + 提取器/清单恶意输入用例 |
| static | shell 语法/脚本登记/文档链接/隐私/构建器输入 | check-docs.sh、fbterm 静态 | 覆盖已全，补"新脚本必登记"由 check-docs 强制 |
| integration | staging/DKMS 离线/包边界/可复现/oracle | 包边界、oracle、parity、离线编译 | 可复现双构建入 CI 需内核头（本机可跑） |
| runtime | 真机 DRM/fbdev/Xorg/GL/音频/Picom/显示/回退 | Phase 4 人工清单（A1-A12） | **新增** `tests/runtime/run-capability-baseline.sh`（授权后执行） |

## 三、显卡标准能力域（12 项，枚举 vs 实际执行必须分开）

| # | 能力域 | 枚举测试（离线/只读可跑） | 实际执行（需真机+授权） | 当前结论 |
| --- | --- | --- | --- | --- |
| 1 | PCI/内核驱动 | `lspci`、dkms status、modinfo vermagic | dmesg PVR/固件/错误计数 | OBSERVED PASS（Phase 4） |
| 2 | DRM/KMS | `ls /dev/dri`、drm_info、sysfs | modeset/热插拔/分辨率切换 | OBSERVED PASS（card0/renderD128） |
| 3 | fbdev/控制台 | `ls /dev/fb0`、fb ioctl | 真实 VT fbterm 绘制/清屏/重入 | OBSERVED PASS（历史 VT） |
| 4 | EGL/GBM/DRI | tools/probe-egl-gbm、probe-x11-egl-gles2 | buffer 分配/DMA-BUF 导入导出 | OBSERVED PASS（hwgl 链路） |
| 5 | OpenGL/GLX/GLES | `glxinfo`/check-desktop-hwgl | 最小 GL 程序（非 llvmpipe） | OBSERVED PASS（4.3 core/ES 3.2） |
| 6 | Vulkan | tools/probe-vulkan-devices | 创建 instance/device + 最小渲染 | OBSERVED 枚举（1.3.264）；渲染 UNVERIFIED |
| 7 | OpenCL/计算 | tools/probe-opencl-devices | 最小 kernel/buffer 读写 | OBSERVED 枚举（3.0/2CU）；执行 UNVERIFIED |
| 8 | 视频 | tools/probe-vaapi / vainfo | 最小 H264/HEVC 硬解；**编码无能力→记录不支持** | OBSERVED 解码；编码不支持（明确记录） |
| 9 | DMA-BUF/同步 | 静态审计（patch-023/025/027） | DRI3/PRIME 自导入、fence、失败路径 | OBSERVED PASS（Phase 4 回归） |
| 10 | 显示输出 | xrandr/DRM 拓扑交叉核对 | 内置屏/外接/插拔/合盖恢复 | OBSERVED PASS（HDMI-2 布局） |
| 11 | 桌面合成/应用 | Picom GLX 静态 | 透明/圆角/拖拽/WebKit DMA-BUF | OBSERVED PASS（Picom GLX） |
| 12 | 音频/显示音频 | aplay -l、wpctl status | 实际播放 HDA + FH2M HDMI | OBSERVED PASS（默认 sink HDA） |

**规则**：枚举成功 ≠ 渲染成功；能力不存在/工具缺失/无显示器/无真 VT → 只允许 `SKIP`/`UNVERIFIED`，
不得伪造 PASS。

## 四、标准输出基线表（`tests/runtime/` 输出规范）

| 能力 | 工具/接口 | 必须记录的标准输出 | 实际使用验证 | 当前结论 |
| --- | --- | --- | --- | --- |
| DRM/KMS | drm_info、sysfs、/dev/dri | card/render、CRTC、connector、mode、plane | modeset/显示切换 | PASS |
| fbdev | /dev/fb0、fb ioctl、真 VT | 节点、分辨率、mmap/ioctl | fbterm 绘制和重入 | PASS |
| GL/GLX/GLES | glxinfo、eglinfo | renderer、vendor、direct、accelerated、版本 | 最小 GL 程序 | PASS |
| EGL/GBM/DRI | eglinfo、GBM/DRI 探针 | vendor、extensions、device、DMA-BUF | buffer 分配/导入 | PASS |
| Vulkan | vulkaninfo | device、API、queue、memory、surface | 创建 instance/device | 枚举 PASS / 渲染 UNVERIFIED |
| OpenCL | clinfo | platform、device、version、extensions | 最小 kernel/buffer | 枚举 PASS / 执行 UNVERIFIED |
| VA-API | vainfo | vendor、profile、entrypoint | 最小硬解 | 解码 PASS / 编码不支持 |
| 音频 | aplay、wpctl | ALSA card、PCM、PipeWire sink | 实际播放 | PASS |

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

当前套件（40 项，全部 CI/沙箱可跑）：fbterm_static×1、picom_install×3、picom_session×3、
xdisplay_install×5、package_boundary×7、manifest×8、version×6、extractor×7。

## 六、覆盖清单（5.md 要求逐项落实）

**fixture 至少覆盖**：缺少 source deb、源包哈希错误、manifest 缺字段、重复路径、绝对路径、
`../` 穿越、非法 kind/link_target、缺失/错误 vendor 文件、中断恢复、`--check-only` 缺失文件必须失败。
→ **已全部落地测试**：`tests/unit/run-manifest-tests.sh`（8 项恶意清单，fixtures/）与
`tests/unit/run-extractor-tests.sh`（7 项：vendor 缺失时 --check-only 失败、完整提取、幂等重跑、
提取后 --check-only 通过、哈希篡改 --check-only 失败、中断/残留重建、源 deb SHA 不匹配失败；
使用临时 fixture deb 与隔离 vendor 树，通过提取器新增的 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖）。

**构建测试至少覆盖**：headers 缺失、`SOURCE_DATE_EPOCH` 缺失、版本低于 patched-27、helper 缺失、
`.o.cmd` 进入包、双构建哈希一致、oracle 文件清单/载荷/DKMS 源码/模块符号一致。
→ 大部分由构建器门禁 + oracle 脚本覆盖（CONFIRMED）；**补"helper 缺失"与"headers 缺失"的
失败用例**（fixture，不实际构建）。

**文档测试至少覆盖**：链接存在、脚本登记完整、README 当前版本准确、无个人路径/token/serverauth、
历史版本不冒充当前、Phase 5 未批准前不宣称旧流程已删除。→ check-docs.sh 已全部覆盖（CONFIRMED）。

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

## 十、runtime 实际覆盖（2026-08-24 沙箱实测）

- 35 项：静态/探测类 15 PASS（PCI/包版本/DKMS/vermagic/模块/参数/proc 状态/固件/错误计数/
  dma_resv 源码存在性等），设备类 19 SKIP（无 /dev/dri 或人工执行项），1 UNVERIFIED（沙箱 GL
  为 llvmpipe）。
- 命名约定：`dmabuf_source_fix_present` 明确为**源码存在性**检查，不是 DMA-BUF 运行能力；
  真实 DMA-BUF 回归单独为 SKIP/UNVERIFIED（人工执行项）。
- 工具缺失（vulkaninfo/clinfo/vainfo/glxinfo/drm_info/xrandr/wpctl）→ SKIP reason=tool_missing；
  工具存在但无 DRI 节点初始化失败 → SKIP；枚举失败不笼统隐藏。
- 未覆盖（人工执行+监督授权后，经 --results-file 合并）：真实 VT fbterm、EGL/GBM 实际绘制、
  Vulkan/OpenCL 最小执行、VA-API 硬解、DMA-BUF 回归探针、modeset/热插拔/合盖、Picom GLX、音频播放。
- `--results-file` 严格解析（2026-08-24）：接受全部已定义项（含 egl_x11_probe/gl_execution）、
  未知名/状态告警忽略、重复名取最后、粘连行拒绝、PASS/FAIL 必须带证据 reason、缺失文件 rc=2、
  强制 `--allow-authorized-tests`；16 项 fixture 测试（tests/unit/run-results-parser-tests.sh）。
- 真机证据合并（2026-08-24 用户实测）：fbterm_real_vt=PASS、egl_x11_probe=PASS、
  gl_execution=PASS（check-desktop-hwgl PASS_DESKTOP_HWGL、Fantasy II-M/DRI2/DRI3/Present/AIGLX）；
  其余人工项按实际证据 UNVERIFIED（枚举/部分完成）；汇总 18 PASS/9 SKIP/8 UNVERIFIED，
  overall=UNVERIFIED（不伪造完整 PASS）。

## 九、风险与未覆盖

- 完整 DKMS 构建需本机内核头 → CI 无法跑 integration 编译（标记 SKIP 而非 PASS）。
- Vulkan/OpenCL 最小执行、编码能力、多屏矩阵：当前 UNVERIFIED/SKIP（工具或授权缺失）。
- 真机能力基线脚本化需监督授权（涉及 /dev/dri 与 X 会话）。

## 证据索引

`tests/README.md`、`scripts/check-docs.sh`、`docs/planning/capability-survey.md`、
`docs/planning/phase4-device-validation.md`。
