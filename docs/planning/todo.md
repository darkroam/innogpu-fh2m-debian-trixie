# 当前 TODO

## 文档与维护

- [ ] **发布阻断：驱动源码与载荷许可证审计**。确认 3 个 `Strictly Confidential` 文件的再分发权，
  补齐源码头引用的许可证文本，完成 484 个源码文件与 192 项 manifest 载荷的逐项清单；关闭前不得
  宣称整个导入源码树开源或发布对应源码/载荷附件。见
  [source-license-audit.md](../project/source-license-audit.md)。
- [x] 入口文档重构（2026-08-21）：README 按 7 项要求精简（适配/演进/bug 清单/安装/文档结构/致谢/许可），
  LICENSE 注释更新为迁移后现状，过期表述清理；与 4.0.0-i1 现状对齐。
- [ ] 为每次新候选包建立独立的 `docs/patches/` 说明和 `docs/incidents/` 验收记录。
- [ ] 将长期维护所需的脚本参数逐步收敛为可审查的配置，保持 `scripts/<name>` 兼容入口不变。
- [x] 完成 2026-08-20 技术 release 审阅：确认 `debs/` 包、版本、哈希和验证证据一致；该结论不
  覆盖 2026-08-24 发现的许可证发布阻断。
- [x] 实际演练 patched-17 回退：安装、重启、验证，再恢复 patched-23；两次重启后的 TTY、Xorg/dwm、
  DRM/fbdev、软件 llvmpipe 和硬件 GL 恢复均通过。
- [x] 完成 release 审阅主体：tag、哈希、包边界、可复现构建（含目录 mtime 修复）、回退路径和附件边界，
  见 [release-review-2026-08-20.md](release-review-2026-08-20.md)。
- [ ] 剩余发布工作：跨硬件实机矩阵（扩展坞/多屏/无盖桌面/其他机型）、电源/合盖矩阵、release 附件上传。
- [x] 源码树迁移阶段 0-4（监督指南 `docs/planning/migration-supervision.md`（监督分支 migration/supervised-source-tree @ bd76e91）管辖）：
  阶段 0 设计冻结 ✅；阶段 1 drivers/ 导入 + 9 patch 转提交 + parity ✅；阶段 2 binary-manifest.json +
  幂等提取 + staging 内核编译 ✅（G1-G7 全 PASS）；阶段 3 新构建器 4.0.0-i1 并行验证 ✅
  （oracle 全 PASS 含 module_symbols、.o.cmd 边界裁定、可复现构建，2026-08-21 监督评审通过）；
  阶段 4 实机候选验证 ✅（2026-08-21：A1–A12 全 PASS、p27 回退演练 PASS、设备推进至 4.0.0-i1）。
- [x] 源码树迁移阶段 5 第一步：旧构建器/wrapper 标记 deprecated + 文档/检查脚本同步（2026-08-21 监督通过）；
  第二步设计（一个发布周期 + 新设备 clone 安装 + 4.0.0-i1→p27 恢复演练后评估移入 legacy/）见
  [phase5-retirement-design.md](phase5-retirement-design.md)，**未批准不执行**。

## 当前活动项

### WebKit DMA-BUF 调查

- [x] 按 [`webkit-dmabuf-investigation.md`](webkit-dmabuf-investigation.md) 区分 DRM vblank、
  GEM/PRIME 隐式同步和预编译 GBM/EGL 用户态问题。
- [x] 完成不 modeset 的 vblank、KMS 拓扑和私有 CPU_PREP 路径探测。
- [x] 用独立 PDP 探针确认 invisible READ mapping 的 `munmap` 无条件逐页回写缺陷。
- [x] 制作只跳过 READ `SYS2GDDR` 的 `patch-023` / `patched-23` 离线候选，并补充 WRITE 回归探针。
- [x] 离线调查期间保持 `patched-22` 不变；未安装候选、未更新 initramfs、未重启。
- [x] 记录 p22/p21 包哈希和恢复命令。
- [x] 部署后完成 READ/WRITE 最小探针和完整基础图形回归。
- [x] 启动 Clash Verge 后完成 DMA-BUF 开启/禁用的应用级启动态 CPU A/B；本次未复现历史 2860% 忙等，
  但启用 DMA-BUF 的主进程占用仍明显较高，因此继续保留禁用 DMA-BUF 的启动包装脚本。
- [x] 完成 p23 invisible READ 成本的 1/4/8/16 MiB 尺寸缩放基线；确认主要成本按页数增长。
- [x] 完成 page stride 1/2/4/16 访问模式基线；确认顺序访问适合受控预取，稀疏访问不能盲目预取。
- [x] 定位 invisible READ 的 page fault/DMA 热点；DMA 描述符和 completion wait 位于预编译
  `innodma.o_shipped`，超出本项目可维护源码范围，不制作 READ 预取候选；patched-24 仅处理
  Debian 6.12.101+ DKMS API 兼容。
- [ ] 将可复现的热点、perf 数据和应用级 workaround 整理为上游/厂商修复报告。
- [x] tests/runtime/ 真机能力基线第一轮（~/4.md，2026-08-24 完成）：
  - [x] 实现 run-capability-baseline.sh（12 能力域、枚举/执行分离、PASS/FAIL/SKIP/UNVERIFIED、脱敏摘要）；
    沙箱实测 35 项：15 PASS / 19 SKIP / 1 UNVERIFIED（无 /dev/dri 时设备项不伪造 PASS）。
  - [x] tests/runtime/README.md（每项权限/设备/X11/TTY/副作用/恢复）；
  - [x] 沙箱验证只读部分 + 真机授权部分（VT/X11/modeset/播放）标注。
- [x] tests/runtime/ 结果合并修正（~/4.md 第二轮，2026-08-24 完成）：
  - [x] 修复 --results-file：接受全部已定义项（egl_x11_probe/gl_execution 等）、严格解析
    （未知名/状态告警忽略、重复取最后、粘连拒绝、PASS/FAIL 必须带证据）、缺失文件 rc=2、
    强制 --allow-authorized-tests；新增 16 项 fixture 测试（run-results-parser-tests.sh；2026-08-24
    证据封存轮追加 `#` 注释跳过 3 项，当前合计 19 项）。
  - [x] 真机证据合并：fbterm_real_vt/egl_x11_probe/gl_execution PASS；解析器测试隔离正式摘要。
- [x] Vulkan/OpenCL 最小执行能力验证（~/5.md，2026-08-24 完成）：
  - [x] 探针新增 execution 模式：probe-vulkan-devices.c exec（instance→GPU 设备→queue→空 cmd buffer+fence
    提交→限时等待→释放）与 probe-opencl-devices.c exec（GPU 设备→context/queue→add kernel→读回→
    逐元素校验→释放）；loader 路径 env 注入；机器可读输出 + 退出码分级；枚举路径保持。
  - [x] 测试 tests/unit/run-exec-probes-tests.sh（12 项，CI 无 /dev/dri）：编译/缺 loader rc=2/
    无设备 rc=3（不伪造 PASS）/枚举回归/超时无残留/机器格式。
  - [x] 真机执行验证（2026-08-24 用户实测）：vulkan exec 5000（queue+fence submit+wait，
    Fantasy II-M 0x35020023）与 opencl exec 1024（add kernel+读回逐元素校验，Fantasy II-M
    0x1ec8）均 PASS；经 --results-file 合并，runtime_vulkan_execution/runtime_opencl_execution
    升级为 PASS；汇总 21 PASS/9 SKIP/5 UNVERIFIED（overall=UNVERIFIED，未覆盖项不冒充）。
- [x] VA-API H.264/HEVC 实际解码验证（~/6.md，2026-08-24 完成）：
  - [x] 实现 tools/run-vaapi-decode-test.sh（--codec h264|hevc|all；lavfi testsrc2 恰好 30 帧 320x240→
    libx264/libx265→软件参考 NV12 framemd5→强制 VAAPI 硬解（hwaccel vaapi + hwaccel_output_format
    vaapi + hwdownload,format=nv12，无软件回退）→真实 framemd5 格式校验（尾换行/#dimensions
    320x240/30 条合法帧记录/NV12 115200/32 位 hex hash）→逐帧 hash 对比；动态定位 1ec8:9810 render
    node + --device 覆盖 + sysfs 身份验证；退出码 0-5（参数/超时校验在设备检测前）；Driver/Firmware
    状态门禁（不可读/非 OK/8 类错误计数增长 → FAIL，独立 gate）；按 --codec 校验编码器/解码器/vainfo
    VLD profile；fixture 钩子须显式 INNOGPU_VAAPI_FIXTURE_MODE=1，fixture 模式不产任何权威命名空间
    PASS——fixture overall PASS 仅表示控制流通过）。
  - [x] 真机执行并合并证据（2026-08-24 监督者于 c7b3a40 上沙箱外运行 `--codec all`）：H.264 Main 与
    HEVC Main 均强制 VA-API 硬解，各 30 帧 320x240 NV12 framemd5 hash 与软件参考一致，Driver/Firmware
    状态门禁通过 → runtime_vaapi_decode 升级 PASS（证据 baselines/runtime-results-20260824.txt）；能力边界
    仅 Main/Main 8-bit 4:2:0，H.264 High/Constrained Baseline、HEVC Main10、编码未验证。
  - [x] 测试 tests/unit/run-vaapi-decode-tests.sh（52 项，fake fixture，CI 无 /dev/dri：参数/工具缺失/设备/
    身份/输入/参考/硬解/超时/帧数/hash/真实格式负例/聚合/硬解参数断言/状态门禁严格解析（pre 缺失/非
    OK/缺字段/非数字/OKAY 冒充/多余 token/重复字段/前导零 08->09 增长/post 失效/逐项增长）/mktemp 失败/
    TERM 信号清理/无残留（限定 TMPDIR）/不污染 baseline；fixture 模式独立命名空间 fixture_*，绝不输出
    vaapi_decode_* 权威行）。
- [x] DMA-BUF 回归工具与测试（~/7.md，2026-08-24 实现完成，真机证据待授权）：
  - [x] tools/run-dmabuf-regression-test.sh 聚合入口：动态 1ec8:9810 render/card 同源发现、PRIME
    同设备 self-import（probe-dmabuf-self-import.c 新）、invisible GEM READ/WRITE+verify（性能门槛
    max≤40ms，依据 p22 71.9-119.4ms vs 修复后 1.7-2.6ms）、topology 动态 CRTC 索引 + active vblank
    （≥10 样本）+ inactive EINVAL 守卫、Driver/Firmware 双快照严格门禁、超时/信号/幂等清理；
    fixture 独立命名空间 fixture_dmabuf_*，零权威 PASS；退出码 0/1/2/3/5。
  - [x] tests/unit/run-dmabuf-regression-tests.sh（137 项，CI 无 /dev/dri）。
  - [ ] 真机执行（需监督授权）并合并证据；仅当 self-import/READ/WRITE/active vblank/状态门禁全
    PASS 且 inactive guard 按拓扑通过或被明确限定时 runtime_dmabuf_regression 升级 PASS；
    foreign import/跨设备 GTT/V4L2/第二 GPU/长期压力/并发保持 UNVERIFIED。
- [ ] runtime 剩余真实能力证据：modeset/热插拔/合盖、Picom GLX backend、
  音频听感确认；当前权威汇总 21 PASS / 9 SKIP / 5 UNVERIFIED。
- [ ] 代码深度分析与测试体系重构（2026-08-21 开始）：
  - [x] 产出 code-analysis.md、frameworks-and-references.md、test-strategy.md；新增 unit 测试
    （manifest 恶意输入 8、版本排序 6、提取器隔离 7）与 fixtures/；全部 tests/ 统一机器格式
    （`<suite>_tNN=PASS/FAIL/SKIP` + 汇总行）。
  - [x] runtime 能力基线和 Vulkan/OpenCL 最小执行脚本化；真机执行证据已合并。
  - [ ] VA-API 未测 profile（H.264 High/Constrained Baseline、HEVC Main10）、编码能力和多屏矩阵继续
    按独立能力项补证据。
  - [ ] 构建失败用例补齐（headers 缺失、helper 缺失、SOURCE_DATE_EPOCH 缺失）为 fixture。

patched-21 已在 [`../patches/patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)
固定输入、补丁集、辅助载荷和验证门槛。本机当前批次已完成：

- [x] 静态检查与 7 项包边界 fixture；
- [x] 从 Deepin 202504 原 deb 构建 p21，并以固定 epoch 重复构建确认逐字一致；
- [x] 记录 control 字段、文件清单审计和 SHA-256；
- [x] 在 p17 回退、DKMS、headers 和磁盘空间均已确认后部署 p21；完成受控重启；
- [x] 完成 p21 的 PVR、DRM/fbdev、Xorg/GLX、真实 VT fbterm、xdisplay、Picom、音频和桌面验收。

后续只保留发布前工作：扩展坞、三块及以上外屏、无盖桌面和其他硬件的实机矩阵（release 审阅已于 2026-08-20 完成，见 [release-review-2026-08-20.md](release-review-2026-08-20.md)）。
patched-17 回退演练已完成。任一验证失败先进入恢复路径和事故记录，不做模块热切换。

显示引擎代码、配置和内部测试已收敛回 dotconfig 维护。本项目当前只保留 Innogpu 设备钩子、会话
接入和安装边界测试；后续 xdisplay 功能不再在本仓库重复实现。跨项目实机矩阵记录在
`suspended.md`。

Picom patch、配置和安装流程已按 `picom-integration.md` 完成吸纳。升级上游 Picom 时需要重新
审查固定基线 patch。

## 逆向工程与能力挖掘

状态：评估与能力普查已落档（[reverse-engineering-assessment.md](reverse-engineering-assessment.md)、
[capability-survey.md](capability-survey.md)）；以下为未实施或剩余项，每项落地前先补设计、验证与回退。

- [x] 能力面普查（静态部分）：RGX 特性表 dump（90 宏）、BVNC 35.V.1632.23、G0M_SOC 变体确认、
  Vulkan 128 唯一扩展/OpenCL 3.0/VA-API codec/IFBC 静态证据，已落档 [capability-survey.md](capability-survey.md)。
- [x] 能力面普查（运行时枚举与图形执行）：Vulkan 1.3.264 / OpenCL 3.0 / GLX 4.3 已确认，
  Vulkan/OpenCL 最小执行 PASS；VA-API H264/HEVC profile/entrypoint 已枚举且 H.264 Main/HEVC Main 实际
  解码验证 PASS（30 帧 320x240 NV12 framemd5 输出校验），结果见
  [capability-survey.md](capability-survey.md)和 [test-strategy.md](../project/test-strategy.md)。
- [ ] 剩余运行时项：DVFS/功耗实测、CORE_ID/BVNC 直接读取、私有 libinno_codec.so 编码接口验证。
- [x] 建立 DDK V119 ↔ 开源参照对照表：[ddk-v119-mapping.md](ddk-v119-mapping.md)。
  （注：2019 开源 DDK 与 Fuchsia KM 当前不可得，对照基于主线 drm/imagination + Mesa pvr；
  组件映射、UAPI/特性/用户态对比已落档，同谱系源码获取后可按节升级逐函数对照。）
- [x] 落地内核接口修复（一）：`dma_resv_usage_rw` 转换修复（patch-025 / patched-25）已实机验证
  通过（Driver/Firmware OK、桌面 HWGL、PDP READ/WRITE 回归）。
- [x] 落地内核接口修复（二）：未活动 CRTC vblank 守卫（patch-026 / patched-26）已实机验证
  通过（CRTC 1 vblank 正常、CRTC 0/2 立即 EINVAL）。
- [x] 落地内核接口修复（三）：foreign DMA-BUF 生命周期（patch-027 / patched-27）已实机验证
  通过（DRI3/PRIME 自导入回归正常；foreign 路径待第二设备）。
- [ ] invisible READ 批量预取候选调研（调用方批量化，不修改 `innodma.o_shipped` 内部；先补设计）。
- [ ] DVFS/功耗实测与调参评估（候选 7）。
- [ ] `inno_apphint.c` 用户态调优评估（候选 5）。
- [ ] 上游 DDK bugfix/性能 patch 移植（候选 6，依赖开源 DDK 可得性）。
- [ ] 用户态调用画像：扩展 `trace-loader.c` 到 GL/VK/OCL 路径（候选 8）。
- [ ] 完成 `innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）符号级分析，评估预编译核心替换路径（远期定向 RE）。
