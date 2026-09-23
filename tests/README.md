# Tests

该目录用于可重复运行的项目脚本和组件边界测试。

Innogpu 只验证设备钩子与 dotconfig 显示引擎的接入边界：

```sh
tests/xdisplay/run-install-tests.sh
```

该测试只写入 `/tmp` 下的临时 HOME，验证缺少 dotconfig 引擎时拒绝复制私有副本、设备钩子安装、
幂等性、已有 watcher 保留和 `xprofile` 符号链接处理；不会启动真实 watcher 或改变显示布局。
xdisplay 的状态机、布局、适配器、配置和自定义布局测试只在 dotconfig 仓库维护。所有权边界见
[`docs/history/display-integration.md`](../docs/history/display-integration.md)。

不得提交测试运行产生的锁文件、日志、runtime 目录或本机绝对路径。

F maintainer 回归入口：`python3 -B tests/unit/run-fantgpu-maintainer-tests.py --work-dir /tmp/r5-phase2-f-i7-synthetic-<id>`。
默认使用生产生成器与 synthetic 工具，验证 K 全集、两调用点配置例外、失败传播、旧 prerm/O 保留、
ABI 文本反例；网络 namespace 必须可用，不编译、不安装宿主。
默认入口还用真实 OpenSSL 执行与原生 runner 共用的 req 参数：复现默认配置路径缺失、
拒绝显式配置缺失、验证显式配置下 DER 证书的 subject/SKI/codeSigning。
仅绑定 OpenSSL 与普通动态库、复制普通 openssl.cnf；独立夹具内生成一次性 key，finally 删除，
不读取或留存私钥、不挂载宿主 SSL 目录。这是配置回归，不代表 N0 全部依赖闭包或新窗口通过。
R28 用户裁定先修复107；当前窗口 K=现场完整8核，首行及回执明示范围；不排除107。
当前i9使用全新 `~/tmp/r5-phase2-f-i7-20260922-offline-01/attempt-09-i9-all-k/`，
持久证据为 `.build/r26-f-i7-20260922-offline-01/attempt-09-i9-all-k/`；沿用批准批号根，旧失败根及清单原位保留。
`--native-preflight --work-dir <attempt-09-i9-all-k>/preflight` 按批次放行与规约 §四执行；一次完成输入/容量/隔离、
真实签署（逐核 sign-file + 普通模块副本的 modinfo 核对）、初始 initrd 生成和解包内容验证，最后才落完整 n0.json。
`--native-run` 使用 attempt-09-i9-all-k 根，先校验同一回执/输入/证书/initrd，再进入 N1；缺项、混批、过期、
旧根或 symlink 均拒绝。A/B 共用 N0 的 key 与初始 initrd，不在 N1 后重复生成；N0 起算总时间盒 6h。
systemd/systemd-udevd/network 与 /etc/ld.so.conf* 精确入身份清单，复制普通文件和链接，未挂载宿主整目录。
`tests/unit/fantgpu_native.py` 为同入口真实工具辅助文件，非通用安装器。
`--native-run --manifest <inputs.json> --reviewed-sha256 <全值>` 只在实现过审与正式窗口放行后使用；
沿用批准临时/证据根、单窗口 6h、每侧完整八核、最多 8 CPU，缺条件非零，不降级网络隔离。
隔离内 image 清单限定为获准 K，生产 postinst 仍枚举现场全集，未删宿主内核或改生产目标规则。
证据保存 strip 前 ABI、签署后模块与 initrd 字节；synthetic PASS 不代表原生 PASS。
解包链接允许包内相对路径（包括生产 helper 的 `../share/`），严格解析后仍须在包内；
绝对路径、越界、断链、循环均拒绝；合并包时核对目标仍在私有根，再替换已有文件链接，
保留 lib→usr/lib 目录布局。默认回归覆盖这些分支及同名链接覆盖。
真实 DKMS 会忽略 strip 返回码；ABI 包装器失败另写哨兵并挂起自身，保持 DKMS 等待，
外层执行器每 0.2 秒检查并终止本批进程组，正常执行结束时再次拒绝哨兵。
回归实际运行忽略 strip 失败的隔离调用者，失败调用紧接下一行即写后继标记，无延时掩盖竞争；
验证 ABI 读取器非零即停止、后继标记未执行；不能仅凭 postinst 的逐核 PASS 宣称 ABI 通过。
attempt-04 已失败并封存：107 核编译暴露冻结源码的 DRM API 不兼容，两个诊断核的 pahole
亦非零。该窗口收尾时失败门修订仅通过隔离回归；后续新窗口不追认旧窗口，已有根拒绝复用。
attempt-05-101 因检查器误将 SKI 与 modinfo sig_key 比较而失败并保留；后者使用 CMS 证书序列号。
共用解析器拒绝缺失/奇数位/重复序列号，真实 OpenSSL 回归核验；N0 对已锁定 cryptd.ko 副本
真实签署并核 signer/sig_key/hash，不编译或加载该模块，不再用文本签署成功替代模块身份验证。
attempt-06-101 两侧生命周期与整 deb 比较通过，但模块 srcversion/build-id 字节不同，整体仍失败。
原 cfg_detect 并发生成头文件的顺序不稳定已在独立隔离诊断复现；修改冻结源身份须另行裁定。
用户随后授权新派生F源：i8仅在cfg_detect全部wait后追加确定排序（不去重、不改宏值），原i6不动。
builder门禁实际调用生产派生函数，核整树/单文件差异、二次应用/异物拒绝、乱序/重复宏保留/写失败。
strip前保留生成头、fantgpu.mod/.mod.c与依赖.cmd清单及原件，A/B核对这些证据及完整模块字节。
07窗口的采集代码依赖/dev/fd而停批保全；现用普通NUL清单供tar读取，新增无/dev/fd真实采集回归，共97项。
临时容量预留 30 GiB、持久 10 GiB，不足须先解决；不自动扩容/清理，失败保留私有根。
真实模式不入自动 CI，不是 R28 宿主安装授权，不能据此重跑 PM。
`--native-n0-test --work-dir ~/tmp/r5-phase2-f-i7-20260922-offline-01/revision-n0-test-20260923-02/preflight`
是独立修订回归，复用同一个完整 N0 准备函数，不编译驱动、不进入 N1-N5；其 purpose=regression
回执不能供正式 run 使用，测试私钥 finally 删除，日志/公开证书/初始 initrd 保留，不与正式根混用。
默认快速回归另覆盖缺失/失败/测试用途/过期/输入漂移/产物漂移回执、旧根与符号链接拒绝、
准备中途失败不得落 PASS。整组真实 N0 成绩须单列，不能用快速回归绿灯代替。
R27 依赖修订：构建/生命周期所需命令逐项追踪完整符号链接链，精确锁定并复制所需
`/etc/alternatives` 链接与已列工具目标，不复制宿主整个 alternatives 或 /etc；N0 增加
工具路径与真实 awk/编译器入口检查。默认回归覆盖断链/循环/越界/目标漂移，以及真实 awk
缺中间链接 rc=127 → 接线后 rc=0；无驱动编译。
编译器内部工具 `/usr/libexec/gcc` 同样锁定/复制；工具门实际编译并运行最小 C/C++ 程序，
验证 cc1/链接器调用链，不以 --version 成功代替可编译。
原生根仅绑定经字符类型及设备号核对的 `/dev/null`(1:3)、`/dev/zero`(1:5) 软件伪设备；
普通文件不能模拟丢弃/EOF语义，会使并行Kbuild探针误判。禁止其余字符/块设备，未绑定整个
宿主/dev、GPU、磁盘、TTY或sysfs。默认synthetic仍用普通夹具，本项仅适用真实工具模式。
`--native-build-test --work-dir ~/tmp/r5-phase2-f-i7-20260922-offline-01/revision-build-test-20260923-<id>`
在全新隔离根调用同一生产 builder（基准核101，最多8 CPU），验证真实编译、ABI与包边界；
不签署、不安装宿主，purpose=regression，不替代对应获准 K 的正式A/B验收。修订及必要隔离回归按
[批内自主规则](../docs/project/multiagent-collab.md#四codex-主动性实现者权限与义务)集中完成与交付，
失败现场保留，正式窗口不热修续跑。

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

单元测试（manifest 恶意输入、版本排序契约、提取器隔离、collab 结构，CI 可跑、无设备依赖）：

```sh
tests/unit/run-manifest-tests.sh
bash tests/unit/run-license-audit-tests.sh
bash tests/unit/run-version-tests.sh
bash tests/unit/run-extractor-tests.sh
bash tests/unit/run-results-parser-tests.sh
bash tests/unit/run-exec-probes-tests.sh
bash tests/unit/run-vaapi-decode-tests.sh
bash tests/unit/run-dmabuf-regression-tests.sh
bash tests/unit/run-drm-topology-tests.sh
bash tests/unit/run-dri-repair-tests.sh
bash tests/unit/run-collab-structure-tests.sh
bash tests/unit/run-check-docs-tests.sh
bash tests/unit/run-suspend-resume-tests.sh
bash tests/unit/run-suspend-failure-finalize-tests.sh
bash tests/unit/run-030-032-pm-probe-tests.sh
bash tests/unit/run-030-033-shipped-abi-tests.sh
bash tests/unit/run-030-034-stop-stage-tests.sh
bash tests/unit/run-fantgpu-pm-probe-removal-tests.sh
bash tests/unit/run-p2-normalize-tests.sh
bash tests/unit/run-r16-gate-tests.sh
bash tests/unit/run-r16-build-bc-map-tests.sh
bash tests/unit/run-r16-classify-tests.sh
bash tests/unit/run-r16-f-payload-integrity-tests.sh
bash tests/unit/run-gen-package-md5sums-tests.sh
bash tests/unit/run-gen-fantgpu-manifest-tests.sh
bash tests/unit/run-validate-fantgpu-manifest-tests.sh
bash tests/unit/run-r16-restore-tests.sh
bash tests/unit/run-materialize-fantgpu-payload-tests.sh
bash tests/unit/run-builder-fantgpu-gates-tests.sh
bash tests/unit/run-check-release-package-tests.sh
bash tests/unit/run-fantgpu-helper-transform-tests.sh
bash tests/unit/run-fantgpu-runtime-health-tests.sh
```

- manifest 测试用 `tools/validate-binary-manifest.py` 对真实清单与 `tests/fixtures/` 下的恶意
  fixture（绝对路径、`../` 穿越、未知 kind、重复目标、缺 sha256、缺 license、链接逃逸、缺失文件）
  断言通过/拒绝；
- suspend/resume 静态测试把跟踪的 HAL/PCI/PVR/DVFS/显示相关源码复制到 `/tmp`，验证
  patch-024/patch-025-display/patch-026-lifecycle/patch-028-temp-monitor/patch-029-ddcci-panel dry-run/应用、025 三行上下文、PowerLock 门禁顺序、
  post-atomic 重复光标恢复移除、单文件范围、`4.0.1-i3`/`4.0.1-i4` 同 epoch 与旧迭代失败关闭、
  lifecycle 的 devfreq drain/PVR 下电顺序、失败回滚、上电后恢复、旧内核防双调；patch-028 的父子 PM
  顺序、多父/多 PVR 子设备、结构尾部 ABI、失败/NO_HARDWARE 门禁、`4.0.2-i1/i2` epoch；零 fuzz/无备份
  应用和 `.orig/.rej` 双树门禁及 patched-28 legacy 开关；patch-029 的 DDCCI panel/backlight/force fixture、i3 epoch，以及 R08 观测器无挂起/显示
  变更动作、固定对象/内核/版本与已加载模块 BTF ABI 失败关闭、primary FB/GEM/scanout 关联、
  shadow/config-valid 时序、HAL 参数语义、entry/return 停止边界及 connector/format 快照；不读取本地载荷、不构建或安装驱动、
  不挂起主机；
- `run-fantgpu-runtime-health-tests.sh` 用合成 dmesg/status/sysfs/devfs 根验证 F 真机前置门禁的
  fail-closed 行为：无 card、固件请求失败、kernel fault 和无 dmesg 均拒绝。它不模拟预编译 HAL
  的真实 bind，也不替代安装后的生产探针和人工特权日志采集；这些限制必须在计划书中明确记录；
- suspend 失败收尾 fixture 验证只有失败证据、回退和重启三个 round ID 绑定标记都为 PASS 时，
  才删除指定 active 指针并保留证据；缺失/错配标记、路径穿越、符号链接根/文件和重复 finalize
  全部失败关闭；active 指针只接受规范化小写 round ID，绝对路径和大写 ID 均拒绝，不需要 root 或真实挂起；
- 030-032 PM 探针静态测试严格回放 i3 锁定快照，覆盖 root/CAP 门、两个精确命令、单 boot
  不重入、sleep/wakeup 与返回值顺序、debugfs 文件引用、PM/remove/probe 同锁、全部 PM 回调及
  shutdown 串行、固定状态 schema、禁止 DMA/PDP 任意调用和禁止异步 timeout；不加载模块或触发 PM；
- 030-033 shipped-object ABI 测试从 i4 快照严格应用修正，要求补丁范围仅为两个 F 源文件、
  `dev_rsrc` 与 i3 锁定快照逐字一致、probe 状态只存在于独立 devres；真实构建另由 builder 的
  `pahole` 门检查模块内结构大小和关键字段偏移；
- 030-034 stop-stage 测试从 i5 快照严格应用诊断补丁，锁定 checkpoint 顺序、退出 errno、
  idle DMA 释放/按需重新获取和 active-channel variant 拒绝；编译实际 write/PCI 回调验证
  成功 stop 才能 re-arm、普通探针不重入与 PM/remove 拒绝。stub 不代表真实子设备恢复，
  不进入 PM；真实恢复仍须监督测试及完整健康门；
- PM 探针发布移除 fixture 门禁覆盖发布源码、snapshot/manifest、builder 树、DKMS 树、模块字符串与
  deb 解包载荷；任一层残留诊断符号、确认 token 或状态字段即失败关闭；
- 许可证审计测试覆盖当前逐文件 inventory 一致性、发布门禁保持 BLOCKED、确定性重建、
  陈旧 inventory、许可证文本缺失、confidential 集合漂移、残缺 Dual MIT/GPL 头、manifest license
  缺失、无证据 SPDX 值、项目 README 声明文字隔离和 `MODULE_LICENSE` 元数据集合漂移；不修改
  `drivers/` 或发布状态。
- 版本测试断言 `4.0.2-i3 > 4.0.2-i2 > 4.0.2-i1 > 4.0.1-i4 > 4.0.1-i3 > 4.0.0-i1 > patched-27`、`1.0.0-i1 < patched-27` 等排序契约；
- package boundary 测试覆盖 `4.0.x-iN` 新架构候选与 legacy patched-N，并拒绝非规范
  `4.0.x-iN`、私有载荷、旧 helper、错误架构、不完整包及 `.orig/.rej` patch 产物；
- 提取器测试用临时 fixture deb 与隔离 vendor 树（提取器支持 `MANIFEST_PATH`/`VENDOR_ROOT` 覆盖），
  覆盖：vendor 缺失时 `--check-only` 必须失败、完整提取、幂等重跑、提取后 `--check-only` 通过、
  哈希篡改 `--check-only` 失败、中断/残留文件重建、源 deb SHA 不匹配失败。
- 结果解析测试覆盖 runtime 脚本 `--results-file` 严格解析：合法合并、未知名/未知状态
  告警忽略、重复名采用最后一条、粘连行拒绝、无尾换行处理、PASS/FAIL 缺证据拒绝、文件缺失 rc=2、
  未授权使用 rc=2、`#` 注释行显式跳过（不告警、不泄漏为结果项）。
- Vulkan/OpenCL 执行探针测试（CI 无 /dev/dri 可跑）：两探针编译、缺失 loader（env 注入）rc=2、
  无设备 rc=3（可解释、不伪造硬件 PASS）、枚举模式仍可用、超时后无残留进程/临时文件、机器可读输出。
- VA-API 解码脚本控制流测试（CI 无 /dev/dri）：fake ffmpeg/vainfo/sysfs 注入（真实 framemd5
  格式 fixture，覆盖 #dimensions/帧数/尺寸/hash/尾换行/坏行/坏 hash/坏尺寸/无尺寸元数据/空输出双零帧/
  空文件）覆盖参数错误（含 --timeout 非数字、缺参值 rc=2）、fixture 钩子缺 INNOGPU_VAAPI_FIXTURE_MODE=1
  rc=2、ffmpeg/vainfo/编码器缺失（按 --codec 分列 libx264/libx265/h264/hevc）、vainfo VLD profile 缺失、
  设备缺失/非字符/PCI 身份不匹配、输入生成/软件参考/硬解失败、超时、帧数/hash 不一致、两 codec 聚合、
  硬解参数断言（-hwaccel_output_format vaapi + hwdownload,format=nv12）、状态门禁严格解析（pre 缺失/pre
  Driver 非 OK/pre Firmware 非 OK/缺计数字段/非数字计数/**OKAY 冒充 OK/bad 0 多余 token/重复字段/前导零
  08->09 增长**/post 失效/8 字段逐项增长）、mktemp 失败 rc=2、TERM 信号清理退出 143 无残留（残留检查
  限定本测试 TMPDIR，不扫描全局 /tmp）、无残留与不污染 baseline；fixture 模式成功输出独立命名空间
  fixture_*（overall=PASS 仅表示控制流通过，rc=0 一致）且逐行 -mode=fixture，**绝不输出任何
  vaapi_decode_* 权威行**。超时判定覆盖 GNU timeout rc=124 与 rc=137（忽略 TERM、由 --kill-after
  最终 SIGKILL 的忙循环 fixture，无后台进程残留）在三阶段（输入生成/软件参考/硬解）均归类为超时
  （整体退出码 5）。
- DRI repair 服务生命周期测试（CI 无 root/systemd//dev）：fake systemctl + 测试根前缀
  （`INNOGPU_DRI_TEST_ROOT`/`INNOGPU_UNINSTALL_TEST`，默认关闭）覆盖源码 fallback helper 安装路径与
  unit `ExecStart` 一致、PATH 注入忽略、失败回滚只删本次新建（已有有效安装在重装失败后保留）、
  外国普通文件/符号链接拒绝覆盖、`enable/start` 失败传播、幂等安装/卸载、package-absent 只清 DRI
  自有路径、版本不匹配零副作用、只删除规范化目标等于本仓库脚本的符号链接（同名外国仓库链接/普通
  文件保留）、测试根安全（空//相对路径 fail closed）、包 helper 分支正例，以及无硬编码目标用户名、
  无 root `$HOME` 回退的静态反例。
- DMA-BUF 回归聚合控制流测试（CI 无 /dev/dri）：fake sysfs/dev/探针注入 + 真实 C 探针契约测试（编译/参数/设备/能力路径 fd 无泄漏）覆盖参数校验（rc=2 设备检测前）、
  fixture 门禁与独立命名空间（零权威 dmabuf_* 行）、编译/cc 缺失、设备发现与身份（缺失/PCI 不匹配/
  card-render 不同源/多目标/非字符设备）、self-import 控制流与能力缺失、READ 输出严格解析（缺行/重复/
  NaN/负数/坏数字/轮数不符/性能越界与覆盖门槛）、WRITE verify 缺失/页数不符、topology（无 active/多 active/
  坏格式/真实形态：active mode 名称为空时 mode=<unnamed> 占位且仍执行 vblank）、active vblank（timeout/fast return/nonadvancing/样本数/乱序/列标题漂移/重复 header/首样本 delta 非零/delta 与序号差不符/kernel_delta 矛盾/内核时间倒退（含最小精度 16.400→16.399）/sequence uint32 越界/summary 指标与样本重算不符/真实 32 位回绕合法）、inactive vblank 守卫（EINVAL 快速通过/
  timeout/错误 errno/过慢/重复 header 或列标题/坏浮点/字段乱序/success=0 时非零 summary/无 inactive 诚实 SKIP）、内核日志门禁（error/GPU hang/dma_buf timeout 均阻断，严重词表驱动覆盖 failure/failures/warn/WARNING/WARN_ON/lockup/wedged 及单复数进行时，debug/installed/hangcheck benign 保持 clean；日志独立状态机：新严重行 FAIL/rc1、post 不可用或截断/重排/插入/无重叠 UNVERIFIED/rc3（一致性失败优先，正 overlap+一致性失败+严重词仍须 UNVERIFIED）、正常环形轮转与完整追加 clean/PASS（多条追加/多条轮转/多新增含严重行均覆盖）、轮转后新增错误 FAIL/rc1、pre 缺失整体 UNVERIFIED）、状态门禁全部负例与错误计数增长、mktemp/外部超时/TERM
  清理无残留（限定 TMPDIR）、不污染 baseline、汇总恒等式与退出码。
- 文档迁移门禁测试（无设备，9 项）：`run-check-docs-tests.sh` 在临时 Git 仓库复制工作树
  （含待收档的新路径），实跑 check-docs；覆盖历史显示引用允许、活动显示引用拒绝、todo
  活动任务拒绝、runtime 汇总缺失、两篇设计的陈旧断言与三篇文档缺失。仅临时仓库建索引，
  不暂存源仓库，不生成构建或运行证据。
- 多 Agent 协作目录结构与隐私测试（无设备）：tools/validate-collab.py 持久化 fixture，覆盖目录命名/
  编号唯一/request+report 模板齐全/INDEX 与目录按编号精确双向一一对应（R01 不误配 R010、重复行、
  孤立行、孤立目录、登记日期/主题与目录一致）/状态白名单/根目录散放文件/根目录或内部符号链接/嵌套目录/仅
  Markdown/INDEX 与隐藏 Markdown 的大小写无关隐私扫描（含 check-docs.sh 的 rg 层共用
  tools/private-data-patterns.txt 模式文件的反例）/缺 INDEX/整个 collab/ 目录缺失视为通过（fresh clone 与 CI）；在临时目录构造，不访问
  仓库真实 collab/。
- P2 规范化映射测试（CI 无 build/ 可跑）：完全隔离（mktemp + trap，`P2_NORM_D_SRC`/`P2_NORM_F_SRC`
  注入合成树，不触碰仓库 `build/`），覆盖合成正例两次逐字节一致且内容正确（确定性 + 规范化路径 +
  F-only 状态）、D 根缺失 rc=3、F 根缺失 rc=3、D/F 存在但无 `.c/.h` rc=4、canonical-path 冲突
  rc=1、多余参数 rc=2；所有失败路径断言退出码、stderr 诊断关键词和未创建输出。
- R16 per-file 门禁测试（CI 无 build/ 可跑，13 项）：完全隔离（mktemp + env 注入 `R16_MANIFEST` /
  `R16_BC_MAP` / `R16_PER_FILE`），覆盖 **G0 production cardinality**（differs=432 + F-only=3
  = 435，任意组成错配 2 例负例拒绝）+ raw-row duplicate canonical path 拒绝 + 未知 manifest
  status 拒绝（白名单 differs/identical/D-only/F-only/F-only-deferred）+ BC 映射数量
  （435/duplicate/BC 覆盖 23）+ per-file 处置覆盖（435/sum/invalid disposition）+ BC/per-file
  一致性（BC tag 不同步拒绝）+ fail-closed 违规（BEHAVIORAL+drop 拒绝）+ malformed manifest
  行拒绝 + `MISSING` 分类拒绝（MISSING+drop 与 MISSING+defer 均拒绝）+ 两次运行字节一致。
- R16 build-bc-map 生成器测试（CI 无 build/ 可跑，10 项）：完全隔离（mktemp + env 注入
  `R16_MANIFEST` / `R16_BC_MAP_OUT`），覆盖合成正例 rc=0 输出 436 行（header + 432 differs +
  3 F-only = 435）+ **production cardinality 硬校验**（differs != 432 或 F-only != 3 立即
  FATAL；含 435 differs + 0 F-only 错配 + 431 differs + 3 F-only 短缺 2 例负例）+ raw-row
  duplicate canonical path 拒绝 + 未知 status 拒绝（白名单同上）+ malformed manifest 行 rc=1
  无输出 + 空 differs rc=1 无输出 + **fail-closed 输出纪律**（预存在输出文件 SHA-256 在失败后
  保持不变；不仅断言"未创建"，还断言"已存在文件未被覆盖"）+ 两次运行字节一致。
- R16 classify 生成器测试（CI 无 build/ 可跑，10 项）：完全隔离（mktemp + env 注入 `R16_MANIFEST` /
  `R16_D_SRC` / `R16_F_SRC` / `R16_D_LICENSE` / `R16_F_LICENSE` / `R16_OUT_DIR`），覆盖合成
  正例 3 differs（PURE_RENAME/BEHAVIORAL/F-ONLY）→ per-file + per-bc 均写入 +
  **production cardinality 硬校验**（differs != 432 或 F-only != 3 立即 FATAL；含 434 differs +
  0 F-only 错配 1 例负例）+ raw-row duplicate canonical path 拒绝 + 未知 status 拒绝 +
  malformed manifest rc=1 + 空 differs rc=1 + 两次运行字节一致 + 缺失 F 内容 rc=1 无输出 +
  **fail-closed 输出纪律**（预存在 per-file + per-bc 双输出文件 SHA-256 在失败后保持不变；
  不仅断言"未创建"，还断言"已存在文件未被覆盖"）+ **schema 字段数硬断言**（per-file 严格 8
  字段、per-bc 严格 9 字段，header 与每行 tab 分隔字段数逐行 `awk` 检查，header 字面值
  锁定列名顺序）。

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

R28 i9：构建器30项含生产派生函数与新旧DRM对象所有权/NULL/ERR_PTR可运行回归；maintainer默认98项，增加完整DWARF读取器的返回码、缺失、多义和布局漂移反例。正式八核产物结果以本批证据为准；历史单核通过与旧八核失败不改写。
