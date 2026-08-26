# 维护策略

## 开发契约与不可变规则

本节是后续维护、重构和自动化 Agent 的最高优先级约束。除非先完成架构评审并同步更新本文档，
任何代码、补丁、测试或文档修改都不得违反这些规则。

### 基线与载荷

- 后续驱动候选必须从 `debs/` 中的 Deepin 202504 原包整体重建。**当前新架构（4.0.0-iN）**以
  `drivers/` 源码树直接构建（`scripts/build-innogpu-driver.sh`，不执行 patch 叠加）；legacy
  patched 系构建（`build-deepin-coherent.sh`）保留作 p27 oracle 与版本护栏。DRI、GBM、GLAPI、
  GLVND、Xorg DDX、固件和 maintainer scripts 一律不得从历史 patched 包拼接。
- `patched-8` 仅是历史回滚物，`patched-17/18/19` 是回退、故障或候选证据，不是后续实现父版本；
  已验证的 `patched-20` 仍属于诊断候选，不能把诊断日志当作长期默认行为。
- 已发布、安装或形成验收证据的版本号禁止复用。辅助脚本、包清单或 maintainer script 发生变化时，
  即使内核补丁不变也必须提升包版本，并重新建立对应的包边界和运行证据。
- release wrapper 必须固定经过审阅的 `SOURCE_DATE_EPOCH`；同一源码、输入 deb、版本和开关重复构建
  必须生成逐字一致的包。哈希不一致时先定位构建环境或时间戳来源，禁止选择其中一个直接发布。
- 补丁/变换边界：历史内核补丁原件在 `patches/`（溯源与回退复现，不再叠加构建）；当前维护的
  第三方组件补丁与配置在 `components/`（picom、fbterm）；无法表示为源码 diff 的厂商对象变换使用
  `tools/` 下的严格确定性工具；设计、开关、验证和回退写入对应的 `docs/patches/patch-*.md`。
  不得通过复制 `.so`、固件或 `.ko` 绕过构建失败。
- 黑盒载荷边界：`.o_shipped`、用户态库、固件等第三方二进制**不入库**，由 `binary-manifest.json`
  唯一清单管理，`scripts/extract-vendor-binaries.sh` 从 Deepin 原包幂等重建到被忽略的 `vendor/`；
  清单中的 `vendor-binary` 是来源分类，不是许可证名称（见 [licensing.md](licensing.md)）。
- 许可证发布门禁：`drivers/` 含 `Strictly Confidential` 与多种许可证声明；在
  [source-license-audit.md](source-license-audit.md) 的 BLOCKED 状态关闭前，不得发布新的源码归档、
  第三方载荷或声称整个导入源码树开源。新增或改变来源内容必须同步
  `license-audit-policy.json` 和逐文件 inventory，并运行 `python3 tools/audit-licenses.py`。机械审计
  PASS 与发布许可是两件事；release 必须额外通过 `--require-releasable`。

### 代码与接口

- `scripts/<name>` 是稳定入口。移动或重命名脚本前必须扫描安装器、测试、配置、服务、桌面源码
  和文档，并保留兼容包装器；不得为了“整理目录”直接破坏外部调用。
- 通用逻辑与设备特例分离：通用显示、安装和验证代码不得硬编码本机输出名、绝对 home 路径或
  固定模式；本机 connector、modeline 和恢复动作必须以明确钩子存在并写明边界。
- xdisplay 引擎、命令、库、配置、设计文档和内部测试的唯一源码位于 dotconfig。本项目只维护
  `XDISPLAY_INTERNAL_OUTPUTS`、`XDISPLAY_RESTORE_COMMAND`、设备恢复钩子和会话接入，不得再次
  导入引擎副本。
- 改变系统状态的脚本必须明确 root、重启、modeset、卸载和回退风险；只读检查不得偷偷执行
  modeset、热卸载驱动或覆盖活动用户配置。

### 证据、隐私与 release

- 外部 `.deb` 只放 `debs/`，由 `.gitignore` 忽略；`third_party/` 解包目录、原始日志、EDID、
  序列号、凭据、认证文件、主机名和用户名不得提交。`baselines/` 只保留精简、可审查的 PASS/FAIL
  标记与阶段验收审计证据（脱敏后）；原始诊断日志不提交。
- 文档示例使用 `~`、环境变量或 `/tmp` 通用路径，不写入本机绝对 home、临时 `serverauth`、真实
  网络标识或硬件隐私数据。提交前必须执行隐私扫描并人工审查新增证据。
- release 上传是源码提交之外的步骤；新架构构建器输出写入被忽略的 `build/`，legacy patched
  构建输出默认写入 `debs/`；均不得因本地构建把二进制产物重新加入 Git。
- release 前必须先通过 `python3 tools/audit-licenses.py --require-releasable`，再运行
  `scripts/check-release-package.sh`。xdisplay 引擎副本、
  历史 Kylin/实验安装器和直接二进制热补丁入口不得出现在 coherent 发布包中。

### 文档与验证

- 行为修改遵循“文档计划 → 代码/补丁 → 与风险相称的测试 → 文档复核 → 提交”的顺序；失败项
  进入 `incidents/`、`todo.md` 或 `suspended.md`，不能写成已验证。
- 每个新增补丁或安装行为必须有对应 fixture、静态检查或实机门槛；驱动、Xorg/GLX、fbterm、
  Picom 和显示 watcher 的结果必须分别记录，不能用一个“桌面能显示”结论替代全部验证。
- 升级 Deepin 用户态、内核、Picom 或 X11 会话入口时，必须重新检查 ABI/符号、固件完整性、
  兼容入口、回退路径和文档链接。

## 文档优先

每次行为修改必须按以下顺序执行：

1. 先更新 `docs/project/` 或 `docs/planning/`，写明现状、目标、风险、验证和回退。
2. 修改代码、补丁、安装脚本或模板。
3. 执行与风险相称的静态、fixture、运行时和实机验证。
4. 再次复核文档，只把已经通过的行为标记为“当前生效”；失败和未验证项进入 TODO 或 suspended。
5. 提交前检查链接、真实路径、术语、版本、脚本接口和个人信息。

## 文档职责

- 根 `README.md` 是唯一入口，只保存当前结论、快速开始和文档导航。
- `project/` 描述当前架构、运行关系和维护边界。
- `planning/` 描述计划、实施历史、挂起条件和迁移状态。
- `user/` 提供自洽的安装、验证、使用和恢复步骤。
- `archive/` 只保存不再变化的历史材料。
- `baselines/` 保存精简历史证据，不代替当前运行检查。

同一事实只设一个权威文档。其他文档使用链接，不复制大段内容。目标设计必须显式标注“未实施”，
不能和当前行为混写。

## 代码边界

- Deepin 202504 原包是驱动源码、用户态 ABI 和打包载荷的唯一技术基线；`patched-8` 仅是历史
  回滚物，`patched-17/18` 仅是结果和故障证据，均不得作为后续实现父版本。
- DRI、GBM、GLAPI、GLVND 和 DDX 必须按同一 Deepin 发布整体部署，禁止从历史包局部恢复 `.so`。
- 通用显示代码不得硬编码外屏名称、数量、固定外屏模式或本机绝对路径。
- 本机内屏识别和 modeline 恢复必须保持为明确设备钩子。
- xdisplay 行为修改和状态机测试在 dotconfig 完成；本仓库只测试 Innogpu 接入不覆盖引擎且保持幂等。
- 不从活动 `/etc`、home 配置或隔离目录整包复制；只吸纳已审查、可复用且必要的文件。
- 外部 deb、日志、缓存、EDID、序列号、凭据和临时测试运行目录不得提交。
- 修改安装脚本时必须检查构建包清单、卸载清单和新设备流程是否同步。

## 系统服务与 systemd unit 目录规范

新增或修改任何随项目部署的 systemd unit（服务、定时器、路径单元）必须遵守以下统一规则，供
未来服务复用（现有实例见 [audio-management.md](audio-management.md) 的持久化文件清单）：

### 目录选择

- **系统包单元（vendor/包管理）**：随 Debian 包或仓库源码构建交付、由包管理器管理生命周期、升级时
  由 maintainer script 处理的单元放 `/usr/lib/systemd/system/`（vendor 只读路径），并在包内声明
  `systemd` 依赖。Debian usrmerge 系统上的 `/lib/systemd/system/` 是兼容路径；导入包保留该布局时
  必须确认它与 `/usr/lib/systemd/system/` 等价。vendor 单元不得放 `/etc/systemd/system/`，那里是管理员本地覆盖区。
- **包管理用户单元（vendor 用户级）**：随包交付、供非 root 用户会话使用的单元放
  `/usr/lib/systemd/user/`（vendor 只读路径），由包管理器管理；用户级 enable 由各用户执行
  `systemctl --user enable`。
- **管理员单元（本机/安装脚本管理）**：由本项目安装脚本直接落地、覆盖 vendor 行为或仅本机生效的
  单元放 `/etc/systemd/system/`（root 可写、优先级高于 vendor 单元）。安装脚本必须显式声明它管理
  该路径，卸载时删除同一路径下的文件。
- **用户单元（非 root 会话）**：仅当前用户会话生效、由本机安装/用户脚本落地的单元放
  `~/.config/systemd/user/`。
- **系统预置用户单元**：需要 root 预置（对所有用户可用）的用户级单元放 `/etc/systemd/user/`；
  它与 `loginctl enable-linger` 没有必然关系——linger 只控制**是否在用户未登录时保持用户服务运行**，
  不决定单元放在哪个目录。需要未登录常驻时单独执行 `loginctl enable-linger <user>` 并记录。
- **可执行辅助**：仅供 root unit 调用的脚本放 `/usr/lib/<项目>/` 或 `/usr/libexec/<项目>/`（包管理）或
  `/usr/local/lib/<项目>/`（本机安装）；仅供用户 unit 调用的脚本放 `~/.local/lib/<项目>/`。需要管理员
  直接调用的稳定命令可放 `/usr/sbin/`（包管理）或 `/usr/local/sbin/`（本机安装），但 unit 的
  `ExecStart` 必须指向实际安装路径，不能依赖 PATH 或在两个前缀间漂移；脚本所有权与 unit 一致。

### enable/start 与 daemon-reload

- 每次写入/删除/修改 unit 文件后必须执行 `systemctl daemon-reload`（用户单元：
  `systemctl --user daemon-reload`），否则旧定义仍生效。
- **enable 与激活必须分开执行**：默认服务安装时先 `systemctl enable <unit>`，再单独执行并记录
  `systemctl start <unit>`（用户单元用 `systemctl --user ...`）；幂等重跑若明确需要重新应用状态，可在
  单独激活步骤使用 `restart`，但失败必须传播且不得冒充首次启动成功。可选服务只在用户明确选择后
  enable。禁止用 `systemctl enable --now` 合并两步。
- 启用前必须检查单元语法：`systemd-analyze verify <unit>`；用户单元写作
  `systemd-analyze --user verify <unit>`（`--user` 位置在 verify 之前，避免解析为单元名）。

### 卸载、回退与所有权

- 卸载脚本必须删除安装脚本创建的全部 unit 文件、辅助脚本和符号链接，并执行 daemon-reload；
  `disable` 后 `stop`，不残留 enable 状态。
- 回退：升级或改动失败时提供明确的旧单元恢复步骤（备份原文件或保留历史版本），并在文档记录回退
  验证命令；不得用禁用全部服务替代针对性回退。
- 所有权与权限：root 管理的单元与文件 `root:root`（`644`/`755`），用户单元归当前用户（`644`/`700`
  视敏感度）；unit 不得以 `User=` 或 `Environment=` 泄露凭据，敏感值必须由 `EnvironmentFile=` 引用
  受限文件或 systemd credentials。
- 任何脚本在修改系统服务状态前必须检查是否以适当身份运行（root/用户），并在文档中写明所需权限。

### 现有实例与已知例外

- `install-hygon-hda-audio.sh` 是本机安装器，系统 unit 位于 `/etc/systemd/system/`，用户 unit 位于
  `~/.config/systemd/user/`，符合 local/admin 所有权；其用户 helper 历史上安装到
  `~/.local/bin/hygon-hda-audio-user-apply`。该路径是现有兼容接口，新服务不得照抄；若迁移到
  `~/.local/lib/<项目>/`，必须同步 unit、卸载清单和回退步骤。当前实现没有对所有被覆盖路径做备份，
  未运行 `systemd-analyze verify`，且用户服务管理失败被忽略；它只满足目录所有权部分，不是完整合规示例。
- `install-dri-node-repair-service.sh` 的包内 helper 路径 `/usr/sbin/innogpu-repair-dri-nodes` 与 unit
  一致；但从源码树直接安装的 fallback 当前写入 `/usr/local/sbin/`，unit 仍固定 `/usr/sbin/`，且启动
  失败会被忽略、卸载器未清理 helper 链接。该 fallback 不能视为已验证安装路径；修复脚本前只记录为
  已知实现缺口，不通过文档命令绕过。
- 当前 deb 从 vendor manifest 导入 `/lib/systemd/system/sw-inno-gl.service` 与 `/usr/sbin/sw-inno-gl`，
  但 control 未声明 `systemd` 依赖，maintainer scripts 也不 enable/start 该单元。它是既有包载荷，
  不是本节规范的合规示例；后续版本必须先决定其保留和生命周期策略，并补包边界测试。

## 实现与文档同步矩阵

实现是行为事实源，文档负责解释所有权、风险和操作流程。修改下列实现时，至少同步检查对应权威文档；
不能只改最接近代码的一份 README，也不能用 planning/history 覆盖当前态：

| 实现变化 | 必须复核的当前权威 | 其他同步项 |
| --- | --- | --- |
| `build-innogpu-driver.sh`、提取器、manifest 或包载荷路径 | `architecture.md`、`dependencies.md`、`docs/user/new-device-install.md` | `scripts/README.md`、迁移设计、包边界测试、恢复文档 |
| 安装/卸载脚本、systemd unit、helper 或持久化配置 | 对应 `project/*-management.md` 与本维护策略 | `scripts/README.md`、用户安装/恢复、卸载清单、fixture 测试 |
| `tools/` 探针参数、输出、退出码或能力边界 | `tools/README.md`、`test-strategy.md` | `tests/README.md`、runtime README、能力调查；真实状态变化还需证据/摘要流程 |
| 测试入口、用例数或 CI 顺序 | `tests/README.md`、`test-strategy.md`、`.github/workflows/ci.yml` | status/todo 中的当前统计；历史计数不回写 |
| patch、`drivers/` 转换提交或外部载荷分类 | `docs/patches/README.md`、对应 patch 文档、`patch-provenance.md` | architecture、manifest、许可证审计；不得擅自关闭 BLOCKED |
| runtime 真机结论 | `baselines/latest-runtime-baseline.txt`（按授权流程生成）和 `status.md` | test-strategy/goals/todo/能力文档；证据文件只追加经审查结果 |

`docs/planning/history.md`、`docs/incidents/`、`docs/archive/` 和历史 baseline 只保存时点事实。发现其中
与当前态不同，应链接当前权威或增加新时点记录，不能重写旧结论。`scripts/check-docs.sh` 是必要护栏，
但当前链接/隐私扫描范围不是全部 tracked Markdown；发布前仍需从 `git ls-files '*.md'` 做全仓复核。

## 提交前检查

至少执行：

```sh
git diff --check
scripts/check-docs.sh
bash -n scripts/*.sh
```

还应检查 Markdown 内部链接和文档中出现的仓库相对路径是否存在。显示接入修改必须运行
`tests/xdisplay/run-install-tests.sh`；xdisplay 引擎行为由 dotconfig 测试，必要的只读实机核对使用
`xdisplay status`。需要 modeset 的验证单独记录实机结果。
