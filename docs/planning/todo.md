# 当前 TODO

## 文档与维护

- [ ] 为每次新候选包建立独立的 `docs/patches/` 说明和 `docs/incidents/` 验收记录。
- [ ] 将长期维护所需的脚本参数逐步收敛为可审查的配置，保持 `scripts/<name>` 兼容入口不变。
- [ ] release 发布前确认 `debs/` 中包与当前状态文档的版本、哈希和验证证据一致。
- [x] 实际演练 patched-17 回退：安装、重启、验证，再恢复 patched-23；两次重启后的 TTY、Xorg/dwm、
  DRM/fbdev、软件 llvmpipe 和硬件 GL 恢复均通过。
- [ ] 完成 release 最终审阅：tag、哈希、包边界、可复现构建、跨硬件限制、回退路径和 release 附件。

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

patched-21 已在 [`../patches/patched-21-release-candidate.md`](../patches/patched-21-release-candidate.md)
固定输入、补丁集、辅助载荷和验证门槛。本机当前批次已完成：

- [x] 静态检查与 7 项包边界 fixture；
- [x] 从 Deepin 202504 原 deb 构建 p21，并以固定 epoch 重复构建确认逐字一致；
- [x] 记录 control 字段、文件清单审计和 SHA-256；
- [x] 在 p17 回退、DKMS、headers 和磁盘空间均已确认后部署 p21；完成受控重启；
- [x] 完成 p21 的 PVR、DRM/fbdev、Xorg/GLX、真实 VT fbterm、xdisplay、Picom、音频和桌面验收。

后续只保留发布前工作：扩展坞、三块及以上外屏、无盖桌面和其他硬件的实机矩阵，以及 release 审阅。
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
- [x] 能力面普查（运行时部分）：Vulkan 1.3.264 / OpenCL 3.0 / GLX 4.3 / VA-API H264+HEVC 硬解已实机
  确认（vainfo/vulkaninfo/clinfo/drm_info 交叉验证），结果见 [capability-survey.md](capability-survey.md)。
- [ ] 剩余运行时项：DVFS/功耗实测、CORE_ID/BVNC 直接读取、私有 libinno_codec.so 编码接口验证。
- [ ] 建立 DDK V119 ↔ 开源 `pvrsrvkm`/Mesa 对照表，产出 `innosrvkm.o_shipped` 语义还原文档。
- [x] 落地内核接口修复（一）：`dma_resv_usage_rw` 转换修复（patch-025 / patched-25）已实机验证
  通过（Driver/Firmware OK、桌面 HWGL、PDP READ/WRITE 回归）。
- [ ] 落地内核接口修复（二）：未活动 CRTC vblank 守卫、foreign DMA-BUF 生命周期，各自独立补丁。
- [ ] invisible READ 批量预取候选调研（调用方批量化，不修改 `innodma.o_shipped` 内部）。
- [ ] DVFS/功耗实测与调参评估。
- [ ] 完成 `innogpu.o_shipped`（HAL）与 `innodma.o_shipped`（DMA）符号级分析，评估预编译核心替换路径。
