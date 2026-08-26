# 实施历史

## 2026-08-26 DMA-BUF 真机回归与 runtime 证据封存

- 以 root 权限在 `1ec8:9810` 真机执行 `tools/run-dmabuf-regression-test.sh`：同设备 PRIME
  self-import、invisible GEM READ/WRITE、活动 CRTC vblank、未活动 CRTC EINVAL 守卫、Driver/Firmware
  双快照与内核日志门禁全部通过；完整参数和机器输出已脱敏封存在
  `baselines/runtime-results-20260824.txt`。
- 证据合并后 `runtime_dmabuf_regression=PASS`，权威摘要更新为 22 PASS / 9 SKIP / 4 UNVERIFIED，
  overall 保持 UNVERIFIED。边界仍仅为同设备 PRIME self-import；foreign/cross-device/GBM/V4L2、
  长期压力与并发未验证。

## 2026-08-24 VA-API 真机解码验证与 runtime 证据封存

- 实现 `tools/run-vaapi-decode-test.sh`（强制 VA-API 硬解 + 真实 framemd5 格式校验 + 软件参考
  hash 对比 + Driver/Firmware 双快照状态门禁，退出码 0-5；fixture 钩子须显式标记并使用独立命名
  空间 fixture_*，绝不输出权威 vaapi_decode_* 行），配套 52 项单元测试与文档同步（提交 c7b3a40）。
- 监督者于真机执行 `bash tools/run-vaapi-decode-test.sh --codec all`：H.264 Main 与 HEVC Main 均
  完成强制 VA-API 硬解，各 30 帧 320x240 NV12 framemd5 hash 与软件参考一致，Driver/Firmware 状态
  门禁通过（七行原始输出以注释形式原样封存于 baselines/runtime-results-20260824.txt）。
- 证据经 --results-file 合并后，权威摘要 20→21 PASS / 9 SKIP / 5 UNVERIFIED（overall=UNVERIFIED）；
  能力边界：仅 Main/Main 8-bit 4:2:0，H.264 High/Constrained Baseline、HEVC Main10、编码、
  播放/长时/并发/4K/性能功耗均未验证。

## 2026-08-20 能力普查、三连正确性修复与 release 审阅

- 完成 FH2M 能力普查（静态 + 实机）：Vulkan 1.3.264 / OpenCL 3.0 / GLX 4.3 / VA-API H264+HEVC
  解码 profile 枚举等确认（实际码流硬解当时未验证；2026-08-24 完成工具实现与真机取证，见上），落档
  [capability-survey.md](capability-survey.md) 与
  [reverse-engineering-assessment.md](reverse-engineering-assessment.md)。
- patched-25（patch-025 dma_resv usage 语义）、patched-26（patch-026 未活动 CRTC vblank 守卫）、
  patched-27（patch-027 foreign DMA-BUF 生命周期）各自独立分支开发，经离线编译、实机验证后
  fast-forward 合并 main 并打 tag。
- release 审阅（[release-review-2026-08-20.md](release-review-2026-08-20.md)）发现 **deb 构建
  不可复现**：dpkg-deb 保留目录实际 mtime，未应用 SOURCE_DATE_EPOCH。修复构建器（构建前归一化
  整树 mtime），p25/26/27 重建为可复现 SHA 并更新 tag。当时当前运行驱动为 patched-27（Phase 4
  后已推进至 4.0.0-i1，见下方 2026-08-21 条目）。

## 2026-08-21 源码树迁移 Phase 0–4 完成、设备推进 4.0.0-i1

- Phase 3 新构建器并行验证通过监督评审（module_symbols 离线逐项对比、.o.cmd 构建产物边界裁定、
  SOURCE_DATE_EPOCH 必填 + 双构建可复现）；Phase 4 实机候选验证完成：B1-B12 基线、A1-A12 初次
  验收全 PASS、p27 回退演练 PASS、重装 4.0.0-i1 并重启，设备最终运行态 = `4.0.0-i1`。
- 迁移分支整体 fast-forward 合并 main 并推送 origin（当前 main/origin 均为 74b1f04）；Phase 5
  （旧流程退役）按监督安排分两步设计评审，见 [phase5-retirement-design.md](phase5-retirement-design.md)。
- 当前设备运行 `4.0.0-i1`；`patched-27` 转为保留的回退基线；p27/17/8 deb、tag 与 patches/ 永久保留。

## 2026-08-14 patched-21 候选定义与 release 护栏

- 第三轮审计发现已验收 p20 deb 仍携带所有权收敛前的 xdisplay/实验辅助文件，因此保留其运行
  结论但禁止发布和同版本重建。
- 公共构建器改为只接受显式且大于 20 的版本，p17-p20 历史 wrapper 改为明确失败的兼容护栏。
- 定义 patched-21：从 Deepin 202504 原包重建，启用 stage-000 和 patch-001/002/006/007，关闭
  patch-003/004/005/008，不继承 p20 deb。
- 增加 release 包边界审计及 `/tmp` fixture，拒绝私有 xdisplay 副本、实验入口、旧版本号和过期
  设备接入脚本。本阶段只允许构建与离线验证，不安装、不重启。
- 首次重复构建暴露 archive 时间戳不稳定，随后要求公共构建器显式接收 `SOURCE_DATE_EPOCH`，p21
  固定为 `1786665600`；最终两份 deb 逐字一致。
- 最终 p21 control 为 `amd64`、`Installed-Size=326128`，文件大小 79,528,788 字节，SHA-256 为
  `15c1fab4b8a0f36985097e3d1651ff43fc09f00a2bda47058c380e1e561384cc`。包边界通过后部署 p21，
  DKMS、签名、depmod 和 initramfs 均完成；不对安装前已加载模块做热切换。
- 受控重启后验证 p21 当前设备运行通过：固件/PVR、DRM/fbdev、当前桌面与隔离 Xorg/GLX、真实 VT
  fbterm、dotconfig xdisplay、Picom 和音频均正常。原始诊断不入 Git，仅保留脱敏摘要。

## 2026-08-14 文档与 release 目录重构

- 将补丁说明拆为 `docs/patches/patch-*.md`，并为 patched-17 framebuffer、patched-18 用户态混配、
  shader 固件、patched-20 运行时和 Picom 能力特例建立独立事故记录。
- 增加当前状态唯一摘要、脚本职责索引和文档阅读入口；旧 `scripts/` 路径保持为稳定 API，不做
  物理搬迁。
- 将 `debs/` 定义为本地 release/构建目录，仅跟踪说明文件，`.deb` 和 `third_party/` 继续忽略。
- 集中整理开发、测试、隐私、载荷基线和回退约束，后续行为修改必须先更新文档再改代码。

## 2026-08-14 显示引擎所有权收敛

- 确认 dotconfig 已将 xdisplay 演进为独立命令和共享库，Innogpu 仓库中的旧单文件副本不再是当前实现。
- 从本仓库移除 xdisplay、displayselect 和引擎内部 fixture，只保留设备恢复钩子、会话接入和边界测试。
- 根 README、架构、显示说明和用户文档统一以 dotconfig 为 xdisplay 唯一源码权威。
- 接入安装器改为要求目标用户预先安装 dotconfig xdisplay，禁止覆盖或降级当前显示引擎。

## 2026-07-08 仓库清理

- 保留 `patched-8` 回退点和 `patched-17` 当前成功点。
- 删除失败或过渡版本、重复日志和包含本机路径的大型原始基线。
- 将外部 deb 改为 release 下载后放到仓库根目录，并由 `.gitignore` 排除（当前已迁移为 `debs/`，
  该条保留为当时的历史状态）。
- 清理脚本中的固定 home 路径，建立 `$INNOGPU_ROOT`、`$INNOGPU_X_USER` 等约定。

完整历史记录见 `../archive/cleanup-20260708.md`。

## 2026-07-09 内置喇叭

- 确认 Innogpu 声卡只负责 DP/HDMI 数字音频。
- 将 PCI `1d94:14c9` 绑定到 `snd_hda_intel`，识别 Conexant SN6180。
- 修复全局 `ALSA_CONFIG_PATH` 导致的 PipeWire/WirePlumber profile 失败。
- 增加系统级和用户级服务，重启后内置喇叭与默认 PipeWire sink 验证通过。
- 对应提交：`9cdc410`。

## 2026-07-16 显示来源实现

本机 dotfiles 仓库完成显示 watcher 改造，来源提交为：

- `0b40ac7 Refactor X11 display observation`
- `1f8790b Document display framebuffer diagnostics`
- `5628c6e Fix dock display convergence`

该实现已在本机通过 fixture、当前 X11、热插拔、开合盖和重启验证。

## 2026-07-16 显示代码吸纳

- 吸纳通用 `xdisplay.sh`、共享锁 `displayselect`、设备恢复钩子和净化后的 fixture。
- 删除旧的硬编码 `xdisplay.sh.with-innogpu-restore`。
- 新增可独立测试的用户会话安装器，并接入 patched-17 安装、软件 Xorg 准备和 deb 构建流程。
- fixture 共 27 项、用户安装器 4 项通过；patched-17 测试包在 `/tmp` 完整重建并核对内容。
- 当前 X11 只读比较确认仓库 watcher 与运行来源一致，合盖外屏 `health=ready`。

完整迁移记录见 `display-integration.md`。

## 2026-07-17 Picom GLX 兼容流程吸纳

- 从 dotfiles 提交 `7747c2d` 和当前配置恢复 Picom/xcompmgr 启动、v13 动画、发白排查、st 不透明
  规则和最终模糊配置的处理历史。
- 从上游 Picom `6d676824` 的本机源码差异导出 explicit uniform location shader 探测 patch。
- 保存与当前运行文件逐字一致的 `picom.conf`，增加独立会话、依赖、构建和用户安装脚本。
- 在 `/tmp` 干净 clone 中完成 patch 和 46 步构建；GLX diagnostics 确认为 Fantasy II-M 硬件加速。
- 用户安装器和会话选择各 3 项通过，当前运行 Picom 未停止或替换。

完整记录见 `picom-integration.md`。

## 2026-08-13 Picom GLX 能力判断修复复核

- 反查 `third_party/` 中的 Innogpu 用户态库：当前仓库只保存预编译 `.so`，没有可直接修改的
  OpenGL 扩展表源码；驱动库字符串和 GLSL 诊断表明 `layout(location = ...)` 编译能力存在，
  但运行时扩展列表未暴露该名称。
- 保持驱动包不变，采用现有 `components/picom/001-probe-explicit-uniform-location.patch`，让
  Picom 在扩展缺失时编译最小 shader，失败仍阻止 GLX，成功才继续。
- Picom release/debugoptimized 构建、单元测试和真实 `DISPLAY=:0` GLX 启动验证通过；运行日志
  只出现兼容 warning。该特例仍由 Picom 用户态 patch 所有，未加入 DKMS deb。
