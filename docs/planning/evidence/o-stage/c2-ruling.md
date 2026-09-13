# C2 三方定案记录：fantgpu 模块 modprobe options

- 文件：docs/planning/evidence/o-stage/c2-ruling.md
- 契约出处：validation-plan（5.0.0-i2-validation-plan.md）§四 C2「fantgpu 模块 modprobe options」
- 采集证据：docs/planning/evidence/o-stage/c2-module-params.txt（同目录，静态采集产物）

## 定案结论（2026-09-13 dsh 终裁修订版，替代此前 no-options 初裁）

- **decision = write-options**
- 判定参数：`firmware_en`，**类型 int**，声明默认 0；**写入值 = 1**
- 定案前提事实（dsh 亲测）：
  - O 血统 builder（`scripts/build-innogpu-driver.sh:389` 附近 postinst）实际写 `options innogpu firmware_en=1` → `/etc/modprobe.d/innogpu.conf`；
  - 真机 `/sys/module/innogpu/parameters/firmware_en` 实测 = **1**（O 机器实际以 firmware_en=1 运行）。
- 判定字段四要素逐项结论：
  1. 参数名与 `firmware_en` 语义对应：**成立**（两侧同名同参，名称相似性不作为等价依据，但此处为逐字同名）；
  2. 类型：**int**（F 侧 `fantgpu/fantgpu_pci_drv.c:308 int 0`；O 侧 `drivers/innogpu/innogpu_pci_drv.c:292-294 int 0`，MODULE_PARM_DESC 逐字相同）；
  3. 用途注释/使用点：两侧各**唯一代码消费点**均在同名函数 `hwinfo_register` 内（重定位证据见下），固件加载开关语义一致；
  4. 与 innogpu `firmware_en` 代码用途对照：**同构**（同上消费点），且与 O 血统实测有效配置（=1）对齐。

## 依据行

- **O 有效配置实测**：O builder postinst 写 `options innogpu firmware_en=1`（`scripts/build-innogpu-driver.sh:389`）；真机 `/sys/module/innogpu/parameters/firmware_en` = 1（dsh 亲测）；
- 声明逐字一致：`firmware_en` 两侧声明均为 `int 0`（F `fantgpu_pci_drv.c:308` / O `innogpu_pci_drv.c:292-294`），MODULE_PARM_DESC 相同；
- 唯一消费点：readelf -r 重定位各 1 条 `R_X86_64_PC32 firmware_en`——F 侧 `fantgpu.o_shipped` 偏移 0xfc08（指令 0xfc05 = `hwinfo_register+0x6c`）、O 侧 `innogpu.o_shipped` 偏移 0xdd86（指令 0xdd83 = `hwinfo_register+0x5a`），均为 `mov 0x0(%rip),%r9d` 载入寄存器；两侧消费点位于同名函数 `hwinfo_register`；
- 默认值同：均为 0（声明默认；生效值由 options 文件决定，两血统均写 1）。

## 三方署名（判定人 + 时间戳）

- **qoder 起草**：2026-09-13 —— 静态采集 c2-module-params.txt + 建议判定条件类型改为 int（validation-plan C2 判定字段②原写 bool，与事实不符）；
- **codex 初审**：2026-09-13 —— 核对重定位/反汇编消费点证据，建议判定条件改为 int（「本次重定位/反汇编证据已通过核对，F/O 分别为 hwinfo_register+0x6c 与 +0x5a，因此 Codex 建议判定条件改为 int；在三方记录更新前，不放行 write-options，也不放行安装回路」）；
- **dsh 终审**：2026-09-13（初裁 no-options，同日**终裁修订**）——**decision=write-options、param=firmware_en、value=1**。修订理由：初裁前提「O 有效配置 = 默认 0」与事实不符——O builder:389 实际写 `options innogpu firmware_en=1`、真机 `/sys/module/innogpu/parameters/firmware_en` 实测 = 1，故 F 侧与 O 血统实测有效配置对齐（同语义、同 desc、同唯一消费点 hwinfo_register）。

## 生效语义（定案回路已生效）

- **options 文件由包内确定性 payload 承载**：builder 组装期写入 `$P/etc/modprobe.d/fantgpu.conf`（内容 `options fantgpu firmware_en=1`，dpkg 管理）——postinst **不写**、remove/purge 由 dpkg 自动移除（符合「本包不创建任何不由 dpkg 管理的文件/链接」契约；codex re-review P1 生命周期缺陷修复）；O 血统保持 postinst 直写历史口径（grandfathered）；
- 定案回路：重构建 5.0.0-i2 → 双构建字节一致 → `build-5.0.0-i2.sha256` 证据 SHA 更新 → validation-plan `package_deb_sha256` 与安装校验 SHA 引用同步；
- 运行时基线断言（`tests/runtime/run-capability-baseline.sh`）两血统同口径期望 `firmware_en=1`，非 1 即 FAIL（禁止隐式漏写 options 或放宽检查绕过）；
- 本记录为真机批前置可定位证据；validation-results 的 C2 条目引用本文件。
