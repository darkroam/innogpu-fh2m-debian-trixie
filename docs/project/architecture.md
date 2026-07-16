# 项目架构

## 目的

本文面向维护者，说明 Innosilicon Fantasy II-M 在 Debian Trixie 上的驱动、显示、图形用户态、
音频和安装脚本之间的职责边界。日常安装与恢复见 `../user/`，未完成工作见 `../planning/`。

## 总体链路

```text
release 中的外部 deb
  -> scripts/install*.sh
       -> DKMS: innogpu.ko + firmware
       -> /dev/dri/card*、renderD*、/dev/fb0
       -> Xorg innogpu DDX + Deepin 202504 GL 用户态
       -> X11 会话中的 xdisplay.sh --watch
            -> lid + DRM connector + RandR
            -> 显示布局与热插拔收敛
       -> patched Picom GLX
            -> 圆角、模糊、动画和窗口合成

PCI 0000:06:00.6 [1d94:14c9]
  -> hygon-hda-audio.service
  -> snd_hda_intel -> Conexant SN6180
  -> ALSA -> PipeWire/WirePlumber
```

## 目录职责

| 路径 | 职责 |
| --- | --- |
| `patches/` | Deepin DKMS 源码针对 Debian 6.12 和本设备的内核补丁 |
| `patches/picom/` | 针对 Innogpu GL 扩展声明缺失的 Picom 源码补丁 |
| `config/` | 项目维护的用户态配置模板，不包含完整个人 dotfiles |
| `scripts/` | 构建、安装、回退、诊断、显示管理、音频固化和验证入口 |
| `tools/` | EGL、GBM、X11 和 loader 的最小探针源码 |
| `tests/` | 可重复运行的静态 fixture 和脚本回归测试 |
| `docs/project/` | 架构、现状、依赖和维护边界 |
| `docs/planning/` | 活动计划、历史、挂起项和迁移记录 |
| `docs/user/` | 新设备安装、日常验证、显示使用和故障恢复 |
| `docs/archive/` | 不再变化但仍有追溯价值的历史记录 |
| `baselines/` | 精简后的 pass/fail 历史证据，不作为当前运行状态来源 |
| `third_party/` | 从外部 Deepin deb 生成的解包目录，不进入 git |

## 驱动与图形用户态

当前成功包为 `innogpu-fh2m-trixie 3.3.3.42-patched-17`。DKMS 源码以 Deepin 202504 为基线，
叠加 Debian 6.12 兼容、G0M PLL、DRM/fbdev 和本地 connector 修复。`patched-8` 保留为显示启动
回退点，不继续修改。

Deepin 202504 deb 同时提供硬件 GL/DDX 用户态。内核模块成功、DRM 节点存在和 Xorg 出图不能
单独证明硬件加速可用；必须分别验证 renderer、direct rendering、DRI、GLX 和 Present。

## 显示管理

显示布局属于 X11 会话，而不是 DKMS 内核模块。目标所有权是：

```text
Xorg/RandR
  -> xprofile 启动 xdisplay.sh --watch
       -> 自动布局、热插拔、开合盖、stale 输出清理

手动入口 displayselect
  -> 与 watcher 共用同一 DISPLAY 的 apply lock
```

通用引擎不得硬编码外屏名称、数量或分辨率。本设备非标准内屏候选和模式恢复通过明确的
Innogpu 钩子注入。仓库已使用通用 `scripts/xdisplay.sh` 替换旧硬编码脚本，并通过
`install-xdisplay-user.sh` 将会话入口安装到目标用户；吸纳和验证记录见
`../planning/display-integration.md`。

## 音频

`innogpu` 注册的 `InnosiliconCard` 是 DP/HDMI 数字音频。本机内置喇叭来自独立 PCI HDA 控制器
`1d94:14c9`，需要显式绑定 `snd_hda_intel`，codec 为 Conexant SN6180。

系统服务负责驱动绑定，用户服务在 PipeWire/WirePlumber 启动后恢复默认 sink 和 mixer。不得全局
导出 `ALSA_CONFIG_PATH` 指向用户配置，因为该变量会替换系统 ALSA 配置并破坏设备 profile 枚举。

## X11 合成器

Picom 使用 Innogpu 硬件 GLX。驱动的 GLSL 编译器能够编译 explicit uniform location，但扩展字符串
没有声明 `GL_ARB_explicit_uniform_location`，上游 Picom 因此在创建 backend 前提前退出。项目
补丁只在最小 shader 实际编译成功时继续，编译失败仍保持上游拒绝行为，不伪造其他 GL 能力。

Picom 属于独立用户态组件，不进入显卡驱动 deb。项目固定上游提交、保存 patch、配置和会话启动
片段；源码仍从上游单独 clone。Picom 未安装时会话片段可回退到 `xcompmgr`。

## 配置边界

- 仓库保存可复用脚本、补丁、模板、验证和必要的设备适配逻辑。
- 活动 `/etc/X11`、logind、udev 状态必须先记录和审查，不能从本机直接整份复制进仓库。
- 用户完整 `xprofile` 包含输入法、Picom、代理等无关设置，不能整体吸纳；只提取显示启动契约。
- 外部 `.deb`、原始日志、EDID、序列号、用户名和绝对 home 路径不得提交。
- `baselines/latest-*` 是历史证据；改变当前机器前仍需本地运行时验证。

## 修改顺序

每次行为修改遵循：先更新对应文档为“计划”，再改代码并验证，最后把文档状态复核为“已生效”或
记录未通过项。完整要求见 `maintenance-policy.md`。
