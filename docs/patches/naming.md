# 补丁命名

新补丁文件名必须匹配 `^(m|g)-[0-9]{3}(-lifecycle)?-[a-z0-9-]+$`。`m` 是必须打上的主线补丁。`g` 是内核胶水，不进 i12 构建器。旧号原样留下，缺号不补。构建器写死清单，禁止 `patches/m-*` 或 `patches/g-*` 通配。

D 线 `patches/001`–`029` 不改名。历史文件里的 `030-NNN` 是当时路径。

## 计数

| 类 | 数量 | 说明 |
|---|---|---|
| m | 21 | 18 链 + `m-035-private-accessor` + `m-036-bf-shf-rename` + `m-051-hwinfo-connector-fallback` |
| g | 14 | `g-037`–`g-050`。本提交入库。入库不等于升格：仍标 g，字节不变，不进 i12 构建器 |
| D | 18 | `patches/001`–`029`，不改名 |
| 未入库 | 0 | 原 14 对 `030-037`–`050` 随本提交改名入库。变更理由是编号规范统一，不是把胶水升成主线 |

`g-050-fb-helper-alloc` 是 6.12 胶水，不是 6.18。

## 映射

| 旧名 | 新名 |
|---|---|
| `030-001` | `m-001-kernel-6-12-compat` |
| `030-002` | `m-002-dp-fbdev-fallback` |
| `030-006` | `m-006-local-connector-acpi-map` |
| `030-007` | `m-007-fbdev-io-mmap` |
| `030-009` | `m-009-local-internal-edp` |
| `030-023` | `m-023-invisible-read-no-writeback` |
| `030-024` | `m-024-suspend-resume` |
| `030-025` | `m-025-dma-resv-usage-rw` |
| `030-026` | `m-026-inactive-crtc-vblank-guard` |
| `030-026-lifecycle` | `m-026-lifecycle-suspend-dvfs` |
| `030-027` | `m-027-foreign-dmabuf-lifecycle` |
| `030-028` | `m-028-hal-temp-monitor-delay` |
| `030-029` | `m-029-ddcci-panel` |
| `030-030` | `m-030-hwinfo-audio-fallback` |
| `030-031` | `m-031-pm-marker` |
| `030-032` | `m-032-pm-probe` |
| `030-033` | `m-033-shipped-abi-devres` |
| `030-034` | `m-034-stop-stage` |
| `030-035` | `m-035-private-accessor` |
| `030-036` | `m-036-bf-shf-rename` |
| `030-037` | `g-037-module-import-ns` |
| `030-038` | `g-038-debugfs-fops-cast` |
| `030-039` | `g-039-timer-delete-sync` |
| `030-040` | `g-040-ida-alloc` |
| `030-041` | `g-041-drm-driver-date` |
| `030-042` | `g-042-hrtimer-setup` |
| `030-043` | `g-043-dma-fence-value-str` |
| `030-044` | `g-044-fill-fb-struct-format` |
| `030-045` | `g-045-gem-fb-create-format` |
| `030-046` | `g-046-pfn-t-shim` |
| `030-047` | `g-047-fbdev-probe` |
| `030-048` | `g-048-gem-lock` |
| `030-049` | `g-049-shipped-obj-copy` |
| `030-050` | `g-050-fb-helper-alloc` |
| `030-051` | `m-051-hwinfo-connector-fallback` |

## 系统×内核矩阵

机器可读表是 [system-kernel-matrix.json](system-kernel-matrix.json)。人读规程在这里，不把行抄进 status。

两张表：

- `current`：已验收的系统×内核。字段是 m 基座、g 集、该行的内核补丁、安装/验收/回滚入口、版本修订号、构建身份（树哈希 + srcversion）、安装说明。Debian 当前是 001–002，Gentoo 当前是 001–006。
- `in_progress`：未完成的系统×内核。字段是阶段、断点（已打补丁、ko 状态、待验证项）、下一步。空表合法。中断后只读这一行就能接着做。

互转：进行中行的待验证项清空，并补齐当前表字段后，移入 `current`，同时从 `in_progress` 删除。当前行被新工作替代时移入 `history`。替代工作未完成则另开进行中行。同一 system+kernel 不能同时在 `current` 和 `in_progress`。

新增行：谁加系统或内核，谁在同一批改这张表、构建脚本和安装说明。先有该内核的干净编译和验收，再写入 `current`。没做完就只写 `in_progress`。`m_base` 和 `g_set` 只列构建脚本实际打上的补丁，不从 meta 的记录内核反推。001–004 逐个重验，禁止复用另一内核的模块。Gentoo `current` 在 2026-10-20 三证后含 `005-drm-display-hdmi-helper-prompt` 与 `006-vmap-pfn-prompt`；Debian `current` 正好是 001–002。002 是机型 workaround，换内核版本必须重适配。不为填列而加转发脚本。

版本：iN 只随 m 链实质变更。g 集、构建器外补丁、内核补丁和构建目录不同，都不加 iN，只改 `build_identity`。`check-docs` 核对引用路径存在；补丁若写了 `meta_kernel`，必须等于该 meta 的 `verified.clean_build.kernel`。meta 没有 applicability 字段时不失败；将来有了，行内核必须落在该字段里。
