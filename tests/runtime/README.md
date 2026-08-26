# Runtime 真机能力基线（4.0.0-i1）

`tests/runtime/run-capability-baseline.sh` 是 FH2M 真机能力的机器可读基线测试（详见
[测试策略](../../docs/project/test-strategy.md)）。**只读默认**；真实 TTY/显示器/modeset/播放/Vulkan-OpenCL-VAAPI-DMA-BUF 执行等有副作用
操作**永不自动运行**：脚本只输出人工命令清单，由你在真实设备会话执行后，把结果写入文件并用
`--results-file` 合并回摘要（或单独记录）。

## 运行方式

```sh
bash tests/runtime/run-capability-baseline.sh                      # 只读默认
bash tests/runtime/run-capability-baseline.sh --allow-authorized-tests   # 仅解锁人工命令清单（不自动执行）
bash tests/runtime/run-capability-baseline.sh \
  --allow-authorized-tests --results-file results.txt   # 合并人工结果（必须两者同时提供，否则 rc=2）
```

`--results-file` 每行一条：`runtime_<name>=PASS|FAIL|SKIP|UNVERIFIED [reason=...]`，**接受脚本定义的
全部测试项**（含 egl_x11_probe、gl_execution 等），覆盖默认 SKIP 条目。严格解析规则（2026-08-24）：

- 未知名（不在脚本定义集）→ 告警并忽略；
- 未知状态 → 告警并忽略；
- 文件内重复名 → 告警并采用最后一条；
- 粘连行/多余字段（reason 内再出现状态令牌或 runtime_）→ 告警并忽略；
- **PASS/FAIL 必须带非空 reason（人工命令/证据），否则不合并**；
- 无尾换行的最后一行正常处理；
- 必须与 `--allow-authorized-tests` 同时使用，否则 rc=2；文件缺失 rc=2。

解析行为由 `tests/unit/run-results-parser-tests.sh`（19 项 fixture）覆盖；`#` 注释行（证据文件内的
说明与原始工具输出注释）显式跳过，不告警、不泄漏为结果项。

输出：每条 `runtime_<name>=PASS|FAIL reason=...|SKIP reason=...|UNVERIFIED reason=...`；
汇总 `runtime_total=... runtime_passed=... runtime_failed=... runtime_skipped=... runtime_unverified=...`
与 `runtime_overall=PASS|FAIL|SKIP|UNVERIFIED`。

退出码：0=PASS，1=FAIL，2=SKIP（无 FAIL/UNVERIFIED），3=UNVERIFIED（无 FAIL）。

证据与隐私：所有输出先写入临时 RAW_LOG（`mktemp` 于 `\${TMPDIR:-/tmp}`），`EXIT` trap 统一脱敏
（绝对 home 路径→`~`、临时 Xauthority 文件名→`AUTHTOKEN.REDACTED`、`XAUTHORITY=`→`REDACTED`）后生成完整日志
`baselines/runtime-baseline-<ts>.txt`（gitignored）并清理临时目录——异常退出也只会留下脱敏产物；
精简摘要 `baselines/latest-runtime-baseline.txt`（跟踪，仅 runtime_ 行与 # 元数据）。

**采集环境与人工证据分离**：摘要头部 `# kernel=... dri=... root=... tested_commit=<hash>` 只反映**本次采集环境**
与**生成基线时被验证的代码提交**（`tested_commit` 记录生成时刻的仓库 HEAD；随后为封存审计结果而提交的
文档变更不会使其失效——它表示生成时验证的代码状态，不是永远等于最终 HEAD）；经 `--results-file`
合并的人工真机证据在摘要中输出 `# evidence_merged=1 source=<file>` 行标注来源，审计时不得把沙箱
环境元数据与人工证据误认为同一次运行产生的完整结果。

**基线生成时机**：`baselines/latest-runtime-baseline.txt` 只允许从**干净提交**重新生成（`tested_commit` 必须
对应被验证的已提交代码状态，见上）；工具代码尚未提交时不得用脏工作树重新生成并宣称某个 commit。
DMA-BUF 聚合入口（`run-dmabuf-regression-test.sh`）随 2026-08-25 提交，基线已从该干净提交重新生成并
封存（`tested_commit=210b274`）；topology `<unnamed>` 修复后从 `ce65ff9` 重新封存，内核日志门禁
状态机修复后从 `86fe2cd` 重新封存，DMA-BUF 真机 PASS 证据提交 `e1e7502` 后再次重新封存（当前
`tested_commit=e1e7502`）；人工命令指向聚合入口，`runtime_dmabuf_regression` 已升级为 **PASS**
（2026-08-26 root 权限真机运行，证据 `baselines/runtime-results-20260824.txt`，22 PASS / 9 SKIP / 4 UNVERIFIED）。

## 环境判定

| 变量 | 判定 |
| --- | --- |
| `dri` | 是否存在 /dev/dri 节点（沙箱/SSH 无 → 设备项 SKIP） |
| `fb` | 是否存在 /dev/fb0 |
| `tty` | 是否在真实 TTY（[ -t 0 ]） |
| `x` | 是否有 DISPLAY |
| `root` | 是否 root（journal/权限项） |

## 每项测试的要求（权限/设备/X11/TTY/重启/副作用/恢复）

| 测试 | 只读 | root | 设备(/dev/dri) | X11 | 真实 TTY | 重启 | 副作用/恢复 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| pci_enumeration / pci_driver_binding | 是 | 否 | 否 | 否 | 否 | 否 | 无 |
| package_version / dkms_status / module_vermagic | 是 | 否 | 否 | 否 | 否 | 否 | 无 |
| module_loaded / module_param_firmware_en | 是 | 否 | 否 | 否 | 否 | 否 | 无 |
| proc_driver_status / proc_firmware_status / proc_error_counts | 是 | 否 | 否 | 否 | 否 | 否 | 无 |
| journal_kernel_errors | 是 | 建议 | 否 | 否 | 否 | 否 | 无（受限时 SKIP） |
| drm_nodes / fbdev_node / drm_topology_enumeration | 是 | 否 | **是** | 否 | 否 | 否 | 无（drm_info 只读） |
| fbterm_real_vt | 否 | 是 | 是 | 否 | **是** | 否 | 需授权；失败 → recovery.md VT 恢复 |
| egl_gbm_probe / egl_x11_probe | 否 | 否 | 是 | egl_x11 需 | 否 | 否 | 编译产物在 mktemp；不污染桌面 |
| gl_enumeration | 是 | 否 | 否（X 查询） | 是 | 否 | 否 | 无；沙箱显示 llvmpipe 时标 UNVERIFIED |
| gl_execution | 否 | 否 | 是 | 是 | 否 | 否 | 运行 check-desktop-hwgl.sh（只读） |
| vulkan_enumeration / vulkan_execution | 是/否 | 否 | 执行需 | 否 | 否 | 否 | 执行：`tools/probe-vulkan-devices.c exec [timeout_ms]`——创建 instance/device/queue，空 cmd buffer+fence 提交并限时等待（默认 5s，可参数覆盖）；无渲染副作用 |
| opencl_enumeration / opencl_execution | 是/否 | 否 | 执行需 | 否 | 否 | 否 | 执行：`tools/probe-opencl-devices.c exec [elements]`——context/queue + add kernel + 阻塞读回 + 逐元素校验；仅读写 buffer |
| vaapi_enumeration / vaapi_decode / vaapi_encode | 是/否 | 否 | 是 | 否 | 否 | 否 | 解码：`bash tools/run-vaapi-decode-test.sh --codec all`（强制 VAAPI 硬解 + 真实 framemd5 格式校验 + 软件参考 hash 对比 + Driver/Firmware 双快照状态门禁，退出码 0-5）；**真机已执行并合并**：H.264 Main + HEVC Main 各 30 帧 320x240 NV12 framemd5 一致 → runtime_vaapi_decode=PASS（证据 baselines/runtime-results-20260824.txt）；fixture 钩子输出独立命名空间 fixture_*（绝不产生 vaapi_decode_* 权威行）；编码无实现 → UNVERIFIED/不支持 |
| dmabuf_fix_present / dmabuf_regression | 是/否 | 回归通常需 | 回归需 | 否 | 否 | 否 | 回归：`bash tools/run-dmabuf-regression-test.sh [--render-device NODE] [--card-device NODE]`（同设备 PRIME self-import + invisible GEM READ/WRITE + vblank guard + 状态门禁；退出码 0-5；能力边界：仅同设备 PRIME self-import，foreign/cross-device/GBM/V4L2/长期压力/并发保持 UNVERIFIED）；**真机已执行并合并**：2026-08-26 root 权限运行 → runtime_dmabuf_regression=PASS（证据 baselines/runtime-results-20260824.txt）；普通用户无法读取 dmesg 时日志门禁不能得到 clean，整体只能 UNVERIFIED；探针可能占用 GPU，需授权 + 超时 |
| display_topology / display_modeset | 是/否 | 否 | 是 | 拓扑需 | 否 | modeset 需 | modeset/热插拔/合盖需授权 |
| picom_running / picom_glx | 是/否 | 否 | 否 | glx 需 | 否 | 否 | 只读状态；glx backend 需授权 |
| audio_cards_enumeration / audio_default_sink / audio_playback | 是/否 | 否 | 否 | 否 | 播放需 | 否 | 播放需授权（aplay 测试音） |

## Vulkan/OpenCL 最小执行（2026-08-24）

探针执行模式（dlopen、无 Vulkan/OpenCL 头文件）：

```sh
gcc -O2 -o /tmp/pvk tools/probe-vulkan-devices.c -ldl && /tmp/pvk exec [timeout_ms]
gcc -O2 -o /tmp/pocl tools/probe-opencl-devices.c -ldl && /tmp/pocl exec [elements]
```

- **Vulkan**：创建 instance → 选 GPU 物理设备（优先 Innosilicon 0x1ec8，拒绝仅 CPU/software）→
  device/queue → 空 command buffer + fence 提交 → 限时等待（默认 5s）→ 逆序释放。
  退出码：0=PASS 2=loader 3=无 GPU/初始化 4=device/queue 5=submit/wait。
- **OpenCL**：选 Innosilicon GPU device → context/queue → 输入/输出 buffer → add kernel 编译运行 →
  阻塞读回 → 逐元素校验 → 逆序释放。退出码：0=PASS 2=loader 3=无 GPU
  4=context/queue/buffer 5=build 6=run 7=verify。
- loader 路径可用 `PROBE_VULKAN_LOADER`/`PROBE_OPENCL_LOADER` 覆盖（测试注入缺失场景）。
- 权限/设备：需真实 `/dev/dri` 与授权；**枚举成功不等于 execution PASS**。无设备时探针输出可解释
  失败（rc=2/3），不伪造硬件 PASS。超时保护为 Vulkan fence 限时加测试外部 `timeout`；探针不创建
  临时文件、不留后台进程（`tests/unit/run-exec-probes-tests.sh` 12 项覆盖）。

## 分级规则

- 枚举成功 ≠ 实际执行成功：`_enumeration` 与 `_execution` 分开记录；
- 沙箱/SSH/无 /dev/dri 时设备项只能 SKIP/UNVERIFIED，**不得伪造 PASS**；
- 缺少工具/权限/显示器/真 TTY → SKIP reason=<原因>，不降级为 PASS；
- 单项失败继续采集其他独立能力，最后按失败项返回总结果。
