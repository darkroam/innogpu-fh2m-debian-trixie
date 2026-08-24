# Runtime 真机能力基线（4.0.0-i1）

`tests/runtime/run-capability-baseline.sh` 是 FH2M 真机能力的机器可读基线测试（详见
`~/4.md`）。**只读默认**；真实 TTY/显示器/modeset/播放/Vulkan-OpenCL-VAAPI-DMA-BUF 执行等有副作用
操作**永不自动运行**：脚本只输出人工命令清单，由你在真实设备会话执行后，把结果写入文件并用
`--results-file` 合并回摘要（或单独记录）。

## 运行方式

```sh
bash tests/runtime/run-capability-baseline.sh                      # 只读默认
bash tests/runtime/run-capability-baseline.sh --allow-authorized-tests   # 仅解锁人工命令清单（不自动执行）
bash tests/runtime/run-capability-baseline.sh --results-file results.txt  # 合并你人工执行的结果
```

`--results-file` 每行一条：`runtime_<name>=PASS|FAIL|SKIP|UNVERIFIED [reason=...]`，仅接受已知授权项
（fbterm_real_vt/display_modeset/audio_playback/picom_glx/vulkan_execution/opencl_execution/
vaapi_decode/dmabuf_regression），覆盖默认 SKIP 条目；未知条目忽略并告警。

输出：每条 `runtime_<name>=PASS|FAIL reason=...|SKIP reason=...|UNVERIFIED reason=...`；
汇总 `runtime_total=... runtime_passed=... runtime_failed=... runtime_skipped=... runtime_unverified=...`
与 `runtime_overall=PASS|FAIL|SKIP|UNVERIFIED`。

退出码：0=PASS，1=FAIL，2=SKIP（无 FAIL/UNVERIFIED），3=UNVERIFIED（无 FAIL）。

证据与隐私：所有输出先写入临时 RAW_LOG（`mktemp` 于 `\${TMPDIR:-/tmp}`），`EXIT` trap 统一脱敏
（`/home/…`→`~`、`serverauth.*`→AUTHTOKEN.REDACTED、`XAUTHORITY=`→REDACTED）后生成完整日志
`baselines/runtime-baseline-<ts>.txt`（gitignored）并清理临时目录——异常退出也只会留下脱敏产物；
精简摘要 `baselines/latest-runtime-baseline.txt`（跟踪，仅 runtime_ 行与 # 元数据）。

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
| vulkan_enumeration / vulkan_execution | 是/否 | 否 | 执行需 | 否 | 否 | 否 | 执行仅创建实例/设备，无渲染副作用 |
| opencl_enumeration / opencl_execution | 是/否 | 否 | 执行需 | 否 | 否 | 否 | 最小 kernel 仅读写 buffer |
| vaapi_enumeration / vaapi_decode / vaapi_encode | 是/否 | 否 | 是 | 否 | 否 | 否 | 仅枚举/最小解码；编码不验证 → UNVERIFIED |
| dmabuf_fix_present / dmabuf_regression | 是/否 | 否 | 回归需 | 否 | 否 | 否 | 探针可能占用 GPU；需授权 + 超时 |
| display_topology / display_modeset | 是/否 | 否 | 是 | 拓扑需 | 否 | modeset 需 | modeset/热插拔/合盖需授权 |
| picom_running / picom_glx | 是/否 | 否 | 否 | glx 需 | 否 | 否 | 只读状态；glx backend 需授权 |
| audio_cards_enumeration / audio_default_sink / audio_playback | 是/否 | 否 | 否 | 否 | 播放需 | 否 | 播放需授权（aplay 测试音） |

## 分级规则（~/4.md）

- 枚举成功 ≠ 实际执行成功：`_enumeration` 与 `_execution` 分开记录；
- 沙箱/SSH/无 /dev/dri 时设备项只能 SKIP/UNVERIFIED，**不得伪造 PASS**；
- 缺少工具/权限/显示器/真 TTY → SKIP reason=<原因>，不降级为 PASS；
- 单项失败继续采集其他独立能力，最后按失败项返回总结果。
