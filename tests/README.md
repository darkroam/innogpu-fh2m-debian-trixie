# Tests

该目录用于可重复运行的项目脚本和组件边界测试。

Innogpu 只验证设备钩子与 dotconfig 显示引擎的接入边界：

```sh
tests/xdisplay/run-install-tests.sh
```

该测试只写入 `/tmp` 下的临时 HOME，验证缺少 dotconfig 引擎时拒绝复制私有副本、设备钩子安装、
幂等性、已有 watcher 保留和 `xprofile` 符号链接处理；不会启动真实 watcher 或改变显示布局。
xdisplay 的状态机、布局、适配器、配置和自定义布局测试只在 dotconfig 仓库维护。所有权边界见
[`docs/planning/display-integration.md`](../docs/planning/display-integration.md)。

不得提交测试运行产生的锁文件、日志、runtime 目录或本机绝对路径。

Picom 用户配置安装器测试：

```sh
tests/picom/run-install-tests.sh
tests/picom/run-session-tests.sh
```

这些测试只写入 `/tmp` 下的临时 HOME，并用假命令验证 Picom 优先级、xcompmgr 回退和单实例，
不启动真实 Picom 或 xcompmgr。

fbterm redraw 补丁静态测试：

```sh
tests/fbterm/run-static-tests.sh
```

该测试检查构建入口语法以及配置开关、偏移复位和命令行接口是否保留，不访问 framebuffer。真实
VT 的长输出、清屏和跨会话测试不能由 mock 替代，结果记录在事故文档中。

Release 包边界测试：

```sh
tests/package/run-boundary-tests.sh
```

该测试在 `/tmp` 中生成最小 fixture deb，验证新版本清洁包通过，以及私有 xdisplay 副本、复用
patched-20 版本号、过期设备接入脚本、不完整 shader、错误架构和缺失 `Installed-Size` 会被
`scripts/check-release-package.sh` 拒绝。它不读取或安装本机 `debs/` 中的真实驱动包，不运行 DKMS，
也不改变活动 Xorg。

单元测试（manifest 恶意输入、版本排序契约、提取器隔离，CI 可跑、无设备依赖）：

```sh
tests/unit/run-manifest-tests.sh
bash tests/unit/run-license-audit-tests.sh
bash tests/unit/run-version-tests.sh
bash tests/unit/run-extractor-tests.sh
bash tests/unit/run-results-parser-tests.sh
bash tests/unit/run-exec-probes-tests.sh
bash tests/unit/run-vaapi-decode-tests.sh
bash tests/unit/run-dmabuf-regression-tests.sh
```

- manifest 测试用 `tools/validate-binary-manifest.py` 对真实清单与 `tests/fixtures/` 下的恶意
  fixture（绝对路径、`../` 穿越、未知 kind、重复目标、缺 sha256、缺 license、链接逃逸、缺失文件）
  断言通过/拒绝；
- 许可证审计测试（11 项）覆盖当前逐文件 inventory 一致性、发布门禁保持 BLOCKED、确定性重建、
  陈旧 inventory、许可证文本缺失、confidential 集合漂移、残缺 Dual MIT/GPL 头、manifest license
  缺失、无证据 SPDX 值、项目 README 声明文字隔离和 `MODULE_LICENSE` 元数据集合漂移；不修改
  `drivers/` 或发布状态。
- 版本测试断言 `4.0.0-i1 > patched-27`、`1.0.0-i1 < patched-27` 等排序契约；
- 提取器测试用临时 fixture deb 与隔离 vendor 树（提取器支持 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖），
  覆盖：vendor 缺失时 `--check-only` 必须失败、完整提取、幂等重跑、提取后 `--check-only` 通过、
  哈希篡改 `--check-only` 失败、中断/残留文件重建、源 deb SHA 不匹配失败。
- 结果解析测试（19 项）覆盖 runtime 脚本 `--results-file` 严格解析：合法合并、未知名/未知状态
  告警忽略、重复名采用最后一条、粘连行拒绝、无尾换行处理、PASS/FAIL 缺证据拒绝、文件缺失 rc=2、
  未授权使用 rc=2、`#` 注释行显式跳过（不告警、不泄漏为结果项）。
- Vulkan/OpenCL 执行探针测试（12 项，CI 无 /dev/dri 可跑）：两探针编译、缺失 loader（env 注入）rc=2、
  无设备 rc=3（可解释、不伪造硬件 PASS）、枚举模式仍可用、超时后无残留进程/临时文件、机器可读输出。
- VA-API 解码脚本控制流测试（52 项，CI 无 /dev/dri）：fake ffmpeg/vainfo/sysfs 注入（真实 framemd5
  格式 fixture，覆盖 #dimensions/帧数/尺寸/hash/尾换行/坏行/坏 hash/坏尺寸/无尺寸元数据/空输出双零帧/
  空文件）覆盖参数错误（含 --timeout 非数字、缺参值 rc=2）、fixture 钩子缺 INNOGPU_VAAPI_FIXTURE_MODE=1
  rc=2、ffmpeg/vainfo/编码器缺失（按 --codec 分列 libx264/libx265/h264/hevc）、vainfo VLD profile 缺失、
  设备缺失/非字符/PCI 身份不匹配、输入生成/软件参考/硬解失败、超时、帧数/hash 不一致、两 codec 聚合、
  硬解参数断言（-hwaccel_output_format vaapi + hwdownload,format=nv12）、状态门禁严格解析（pre 缺失/pre
  Driver 非 OK/pre Firmware 非 OK/缺计数字段/非数字计数/**OKAY 冒充 OK/bad 0 多余 token/重复字段/前导零
  08->09 增长**/post 失效/8 字段逐项增长）、mktemp 失败 rc=2、TERM 信号清理退出 143 无残留（残留检查
  限定本测试 TMPDIR，不扫描全局 /tmp）、无残留与不污染 baseline；fixture 模式成功输出独立命名空间
  fixture_*（overall=PASS 仅表示控制流通过，rc=0 一致）且逐行 -mode=fixture，**绝不输出任何
  vaapi_decode_* 权威行**。
- DMA-BUF 回归聚合控制流测试（147 项，CI 无 /dev/dri）：fake sysfs/dev/探针注入 + 真实 C 探针契约测试（编译/参数/设备/能力路径 fd 无泄漏）覆盖参数校验（rc=2 设备检测前）、
  fixture 门禁与独立命名空间（零权威 dmabuf_* 行）、编译/cc 缺失、设备发现与身份（缺失/PCI 不匹配/
  card-render 不同源/多目标/非字符设备）、self-import 控制流与能力缺失、READ 输出严格解析（缺行/重复/
  NaN/负数/坏数字/轮数不符/性能越界与覆盖门槛）、WRITE verify 缺失/页数不符、topology（无 active/多 active/
  坏格式/真实形态：active mode 名称为空时 mode=<unnamed> 占位且仍执行 vblank）、active vblank（timeout/fast return/nonadvancing/样本数/乱序/列标题漂移/重复 header/首样本 delta 非零/delta 与序号差不符/kernel_delta 矛盾/内核时间倒退（含最小精度 16.400→16.399）/sequence uint32 越界/summary 指标与样本重算不符/真实 32 位回绕合法）、inactive vblank 守卫（EINVAL 快速通过/
  timeout/错误 errno/过慢/重复 header 或列标题/坏浮点/字段乱序/success=0 时非零 summary/无 inactive 诚实 SKIP）、内核日志门禁（error/GPU hang/dma_buf timeout 均阻断，严重词表驱动覆盖 failure/failures/warn/WARNING/WARN_ON/lockup/wedged 及单复数进行时，debug/installed/hangcheck benign 保持 clean；日志独立状态机：新严重行 FAIL/rc1、post 不可用或截断/重排/插入/无重叠 UNVERIFIED/rc3（一致性失败优先，正 overlap+一致性失败+严重词仍须 UNVERIFIED）、正常环形轮转与完整追加 clean/PASS（多条追加/多条轮转/多新增含严重行均覆盖）、轮转后新增错误 FAIL/rc1、pre 缺失整体 UNVERIFIED）、状态门禁全部负例与错误计数增长、mktemp/外部超时/TERM
  清理无残留（限定 TMPDIR）、不污染 baseline、汇总恒等式与退出码。

## 测试矩阵（分层，2026-08-21）

| 层 | 测试 | 前置 | root | 设备 | 重启 | 可运行环境 |
| --- | --- | --- | --- | --- | --- | --- |
| unit | manifest/许可证审计/版本排序/提取器隔离 | python3、git、dpkg、dpkg-deb | 否 | 否 | 否 | CI/沙箱 |
| fixture | package 边界、fixtures/ | dpkg-deb | 否 | 否 | 否 | CI/沙箱 |
| static | check-docs、fbterm 静态 | rg、perl | 否 | 否 | 否 | CI/沙箱 |
| static | picom 安装/会话、xdisplay 安装 | bash、fake HOME | 否 | 否 | 否 | CI/沙箱 |
| integration | parity/oracle/离线 DKMS（scripts/） | 内核头 | 否 | 否 | 否 | 本机 |
| runtime | 能力基线（tests/runtime/run-capability-baseline.sh，12 能力域，只读默认） | 真机/沙箱；设备项需 /dev/dri | 部分 | 是 | 否 | 沙箱只读（SKIP/UNVERIFIED）；真机授权（--allow-authorized-tests + --results-file） |

runtime 详细要求（每项权限/设备/X11/TTY/副作用/恢复）见 [tests/runtime/README.md](runtime/README.md)。

结果格式统一（全部测试已实现，2026-08-21）：每条用例 `<suite>_tNN=PASS` /
`<suite>_tNN=FAIL reason=...` / `<suite>_tNN=SKIP reason=...`，汇总行
`tests_total=N tests_passed=P tests_failed=F tests_skipped=S`；PASS=0 / FAIL=1 / SKIP=2；
SKIP 不得转 PASS；
有副作用测试必须显式参数确认；临时文件用 `mktemp` + `trap`。

测试数量以各脚本运行时输出为准，不在本文复制易过时的计数。
