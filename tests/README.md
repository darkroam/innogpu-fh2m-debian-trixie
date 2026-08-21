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
bash tests/unit/run-version-tests.sh
bash tests/unit/run-extractor-tests.sh
```

- manifest 测试用 `tools/validate-binary-manifest.py` 对真实清单与 `tests/fixtures/` 下的恶意
  fixture（绝对路径、`../` 穿越、未知 kind、重复目标、缺 sha256、链接逃逸、缺失文件）断言通过/拒绝；
- 版本测试断言 `4.0.0-i1 > patched-27`、`1.0.0-i1 < patched-27` 等排序契约；
- 提取器测试用临时 fixture deb 与隔离 vendor 树（提取器支持 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖），
  覆盖：vendor 缺失时 `--check-only` 必须失败、完整提取、幂等重跑、提取后 `--check-only` 通过、
  哈希篡改 `--check-only` 失败、中断/残留文件重建、源 deb SHA 不匹配失败。

## 测试矩阵（分层，2026-08-21）

| 层 | 测试 | 前置 | root | 设备 | 重启 | 可运行环境 |
| --- | --- | --- | --- | --- | --- | --- |
| unit | manifest 校验/版本排序/提取器隔离 | python3、dpkg、dpkg-deb | 否 | 否 | 否 | CI/沙箱 |
| fixture | package 边界、fixtures/ | dpkg-deb | 否 | 否 | 否 | CI/沙箱 |
| static | check-docs、fbterm 静态 | rg、perl | 否 | 否 | 否 | CI/沙箱 |
| static | picom 安装/会话、xdisplay 安装 | bash、fake HOME | 否 | 否 | 否 | CI/沙箱 |
| integration | parity/oracle/离线 DKMS（scripts/） | 内核头 | 否 | 否 | 否 | 本机 |
| runtime | 能力基线（scripts/phase4-baseline-capture + A1-A12 清单） | 真机 | 部分 | 是 | 否 | 真机（授权） |

结果格式统一（全部测试已实现，2026-08-21）：每条用例 `<suite>_tNN=PASS` /
`<suite>_tNN=FAIL reason=...` / `<suite>_tNN=SKIP reason=...`，汇总行
`tests_total=N tests_passed=P tests_failed=F tests_skipped=S`；PASS=0 / FAIL=1 / SKIP=2；
SKIP 不得转 PASS；
有副作用测试必须显式参数确认；临时文件用 `mktemp` + `trap`。

测试数量以各脚本运行时输出为准，不在本文复制易过时的计数。
