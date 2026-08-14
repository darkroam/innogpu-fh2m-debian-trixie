# patched-21：所有权收敛后的首个 release candidate

## 当前状态

`3.3.3.42-patched-21` 是当前源码定义的首个包边界合规候选。它必须从 Deepin 202504 原 deb
重新构建，不从已安装的 patched-20 或任何其他 patched 包复制载荷。

本页把“构建和离线验收”与“安装后的运行验收”作为两个独立阶段记录。构建完成前不得写成包已存在；
离线验收通过也不代表驱动已安装或运行通过。

当前阶段标记：

```text
BUILD: PASS
PACKAGE_BOUNDARY: PASS
REPRODUCIBILITY: PASS
INSTALL: NOT_INSTALLED
REBOOT: NOT_REBOOTED
RUNTIME_VALIDATION: PENDING
```

## 目标

patched-20 已证明完整 Deepin 载荷配合 framebuffer 修复能够通过 PVR、Xorg/GLX 和真实 VT，
但其 deb 仍携带所有权收敛前的旧 xdisplay 与实验辅助入口，且启用了高频 PVR 诊断。p21 的目标是：

1. 保留 p20 已验证的驱动补丁集合，但关闭只用于定位故障的 `patch-008`；
2. 使用当前仓库已经收敛的辅助载荷，不再打包 dotconfig 拥有的 xdisplay 引擎；
3. 使用新版本号重新建立包内容、哈希和后续运行证据之间的一一对应关系；
4. 先验证可复现构建和 release 边界，本阶段不改变当前机器的已安装驱动或 X11 状态。

## 固定输入

唯一允许的输入包为：

```text
文件：debs/innogpu-fh2m_20250421190503-debug_amd64.deb
Package：innogpu-fh2m
Version：20250421190503-debug
Architecture：amd64
SHA-256：b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2
```

构建器会再次读取 Debian control 字段；包名或版本不符时立即失败。SHA-256 是本次候选输入证据，
更换来源文件时必须先审查差异并更新本文，不能仅靠相同文件名认定载荷一致。

## 固定补丁矩阵

入口 `scripts/build-patched21-deepin-release-candidate.sh` 固定以下开关：

| 阶段 | 开关 | p21 | 理由 |
| --- | --- | --- | --- |
| stage-000 | 构建器始终执行 | 启用 | 保留已验证的 G0M 首次 GPU PLL 调用规避；由严格字节工具执行 |
| patch-001 | 构建器始终执行 | 启用 | Debian 6.12 DKMS 兼容基础 |
| patch-002 | `APPLY_DP_FBCON_FALLBACK=1` | 启用 | 保留 DP 启动 fallback |
| patch-003 | `APPLY_PANEL_BACKLIGHT_FALLBACK=0` | 关闭 | 历史面板试验不是当前稳定集合 |
| patch-004 | `APPLY_PANEL_PLATFORM_FALLBACK=0` | 关闭 | 历史平台 fallback 不是当前稳定集合 |
| patch-005 | `APPLY_BACKLIGHT_FORCE_INITIAL_ENABLE=0` | 关闭 | 避免无条件改变面板初始 enable 行为 |
| patch-006 | `APPLY_LOCAL_CONNECTOR_ACPI_MAP=1` | 启用 | 保留本机 ACPI `_DSD`/HAL connector 映射修正 |
| patch-007 | `APPLY_FBDEV_IO_MMAP=1` | 启用 | 保留已通过真实 VT 验证的 fbdev mmap 修复 |
| patch-008 | `APPLY_PVR_INIT_DIAGNOSTIC=0` | 关闭 | 移除 p20 高频诊断日志，不改变 PVR services 调用 |

`patch-006` 修正的是驱动内部 connector 匹配和索引，不直接定义 RandR 输出名；设备输出候选与模式
恢复仍由本仓库接入钩子注入 dotconfig xdisplay。

## 辅助载荷边界

p21 允许打包驱动运行、恢复和 Innogpu 接入所需的当前脚本，包括 DRI 节点恢复、一次性 Xorg 检查、
TTY 恢复、软件 Xorg 恢复和三个 xdisplay 设备接入文件。以下内容明确禁止：

- `xdisplay.sh`、`displayselect` 或 `.local/lib/xdisplay/` 的私有副本；
- `install-kylin-userspace.sh` 与 `install-experimental-hwgl.sh`；
- `patch-skip-first-gpupll.sh` 及其系统命令链接；
- 与当前源码不一致的 `restore-dp1-mode-x11.sh`、`xdisplay-session.sh`、
  `install-xdisplay-user.sh`。

这条边界由 `scripts/check-release-package.sh` 强制执行，并由
`tests/package/run-boundary-tests.sh` 的最小 fixture 覆盖。包内保留设备接入脚本不表示 Innogpu
拥有 xdisplay 引擎；安装器只能连接预先由 dotconfig 安装的命令和库。

## 构建命令

在仓库根目录执行：

```sh
scripts/build-patched21-deepin-release-candidate.sh
```

默认输出：

```text
debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
```

wrapper 不接受补丁开关覆盖；只允许用 `OUT_DEB` 改变输出路径。它同时固定
`SOURCE_DATE_EPOCH=1786665600`（`2026-08-14T00:00:00Z`），使 dpkg 对构建期新文件和 archive
成员使用同一 release 时间戳。公共构建器要求显式版本大于 20 和有效 epoch，因此历史 p17-p20
包装器只返回错误，不能产生同版本异内容包。

## 构建期与离线验收门槛

构建成功必须同时满足：

1. Deepin 原包具有完整 DKMS 源码、四个 `fh2*` firmware/shader 文件和同源 DRI/GBM/GLAPI/DDX；
2. 所有启用补丁无 `.rej`，stage-000 只匹配唯一预期字节或已变换状态；
3. `dpkg-deb` 成功生成 `innogpu-fh2m-trixie 3.3.3.42-patched-21 amd64`，并写入有效的
   `Installed-Size`；
4. `scripts/check-release-package.sh` 通过完整 firmware/shader/ABI 文件、禁止文件和当前接入脚本比较；
5. 重新解包后，关键 vendor ABI 文件和 firmware 与构建树逐字一致；
6. 使用另一路径重复构建，两个 deb 的 SHA-256 和逐字比较均一致；
7. 记录输出 deb 的大小、control 字段和 SHA-256，并确认 `.deb` 仍被 Git 忽略。

## 离线构建结果（2026-08-14）

固定 wrapper 已完成两次独立输出路径构建，结果如下：

```text
Package: innogpu-fh2m-trixie
Version: 3.3.3.42-patched-21
Architecture: amd64
Installed-Size: 326128 KiB
deb file size: 79528788 bytes
SHA-256: 15c1fab4b8a0f36985097e3d1651ff43fc09f00a2bda47058c380e1e561384cc
repeat build cmp: identical
```

离线验收证据：

- `tests/package/run-boundary-tests.sh`：7 项通过；
- `scripts/check-release-package.sh`：`PASS_RELEASE_PACKAGE_BOUNDARIES`；
- 禁止载荷清单匹配数：0；
- 解包源码标记：patch-002、patch-006、patch-007 存在，patch-008 诊断标记不存在；
- p21 deb 被 `/debs/*` 规则忽略，不进入 Git；
- 当前系统查询仍为 `3.3.3.42-patched-20`，证明本轮没有安装 p21。

这些结果只关闭构建与包边界门槛。没有执行 DKMS、安装、模块切换、测试 Xorg 或重启，因此 PVR、
DRM/fbdev、Xorg/GLX、fbterm、显示管理和桌面运行状态仍必须保留为待验收。

## 本次离线审计发现与固化经验

### 相同内容不等于相同包

首次构建和 `/tmp` 重复构建都通过包边界，但 SHA-256 分别为：

```text
782f53b79759b4e5339b663ee70e735f6b9e90276c4048af0fcd08fc8158526d
bdd02b3d298280dd71e1ec253ae2f0f23c5c3efe0cc63dd48d7feddef902510d
```

拆分 deb 的 ar 成员后确认 `debian-binary` 相同，而 `control.tar.xz`、`data.tar.xz` 和 ar 成员时间
不同。根因是构建期新建的 control、maintainer scripts 和辅助脚本使用当前时间，构建器没有导出
`SOURCE_DATE_EPOCH`。这两份包均被后续确定性构建覆盖，不是 p21 release 产物。

修复后公共构建器要求显式 epoch，p21 wrapper 固定 `1786665600`；两次最终构建的 `cmp` 返回 0。
由此形成的 release 契约是：包清单一致只能证明载荷边界，只有完整 deb 逐字一致才能证明当前构建
输入和归档元数据稳定。

### control 字段也属于载荷契约

第一次独立读取 control 时发现 `Installed-Size` 为空。驱动内容本身没有丢失，但该包缺少应有的
Debian 元数据，不能直接把当时的哈希写成最终候选。构建器随后按不含 `DEBIAN/` 的 payload 大小写入
该字段，release gate 同时新增以下拒绝条件：

- 非 `amd64` 架构；
- 缺失或非法 `Installed-Size`；
- 缺少任一 `fh2m/fh2c` firmware 或 shader；
- 缺少同源 DRI、GBM、GLAPI、GLVND 或 DDX 文件。

对应 fixture 从 4 项扩展为 7 项。最终 SHA-256 只对应补齐这些元数据和审计规则后的当前源码。

建议的只读复核命令：

```sh
scripts/check-release-package.sh debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
dpkg-deb -f debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb \
  Package Version Architecture Installed-Size
dpkg-deb -c debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
sha256sum debs/innogpu-fh2m-trixie_3.3.3.42-patched-21.deb
```

上述操作只读包文件，不运行 maintainer scripts、不触发 DKMS、不替换已安装库。

## 安装前门槛

p21 只有在离线证据写回本文并经过审阅后才可进入实机阶段。安装前必须：

- 保留可安装的 patched-17 deb，并确认恢复命令和 SSH/真实 TTY 可用；
- 记录当前 p20 包版本、活动内核、模块、固件/PVR、Xorg 和显示状态；
- 确认目标内核 headers 与 DKMS 工具齐全；
- 停止把包内旧 p20 显示入口当作恢复路径，使用当前仓库脚本和 dotconfig xdisplay；
- 明确安排一次受控重启；不能用热加载结果代替重启验收。

本次 p21 构建任务不执行这些安装动作。

## 安装后的运行验收

未来安装并重启后，必须按 [`../user/verification.md`](../user/verification.md) 逐项重新建立证据：

1. dpkg/DKMS/活动模块版本精确为 p21，不能沿用 p20 的运行结果；
2. `fh2m.fw` 和 `fh2m.sh` 加载，PVR 进入 ACTIVE，且不再出现 patch-008 的重复诊断；
3. `card0`、`renderD128`、`fb0` 存在，fbdev mmap 与真实 VT fbterm 通过；
4. 临时 Xorg、`xdpyinfo`、`glxinfo`、当前桌面硬件 GL 全部通过；
5. dotconfig xdisplay 的状态、热插拔/开合盖与 Innogpu 模式恢复钩子正常；
6. Picom、正常桌面和内置音频无回归。

任何一项失败都只能记录为对应子系统未通过，不能用“桌面有画面”覆盖 PVR、GL、fbterm 或显示管理
门槛。

## 回退

运行时失败时优先保留 SSH 或真实 TTY，使用已保存的 patched-17 包和
`scripts/install-patched17-and-check.sh` 回退；显示层异常先停用 Picom/自动 X 启动并恢复软件 Xorg，
不得在未知状态下反复热卸载活动 `innogpu` 模块。完整步骤见
[`../user/recovery.md`](../user/recovery.md)。

回退不会删除 p21 的离线构建证据。失败原因、触发条件和恢复结果应写入新的 `docs/incidents/` 文件，
而不是改写 p20 的历史结论。
