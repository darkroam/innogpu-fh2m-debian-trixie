# 5.0.0-i4 启动 Oops：`dev_rsrc` 共享 ABI 被破坏

## 现象

`5.0.0-i4` 在目标内核 `6.12.101+deb13-amd64` 首启时发生内核 Oops，启动未完成，诊断探针也没有执行。失败回包逐字保留如下：

```text
result=FAIL_BOOT_KERNEL_OOPS
probe_executed=no
rip=fixup_pcie_init+0xe/0x40[fantgpu]
cr2=0x2a9
call_path=fantgpu_pci_driver_init
```

这次事故使 i4 诊断构建作废，后续探针切换到 i5。它不是 R5 挂起问题的修复，当前 `R5=FAIL`、`U1/U2`、`validation-results`、`签发`、`tag` 状态不变。

## 证据

- tracked 回包：`docs/planning/evidence/o-stage/runtime-5.0.0-i4/boot-failure-summary.txt#L1-L14 @ aab4ad8a365d`。其中 `rip`、`cr2`、`probe_executed=no`、根因、修正和冻结字段均为实物内容；文件 SHA-256 为 `54f9a0bf09e4`。
- tracked 修正元数据：`patches/030-033.meta.json#L26-L30 @ aab4ad8a365d` 描述固定偏移 ABI 根因，`patches/030-033.meta.json#L47-L67 @ aab4ad8a365d` 记录零 fuzz 重放、树哈希、源码等价和 `pahole` 静态门。
- R16 原文快照：`.runtime-archive/r17-docs/archive-originals/R16/R16-originals.tar @ fee2c89273b5` 内 `./report.md#L10503-L10514`、`./qoder-notes.md#L8021-L8030`；这两处分别记录 i4→i5 事件链和 qoder 的独立 ABI 核对。
- 归档实物：`.runtime-archive/runtime-5.0.0-i5/i5-101-dev_rsrc.pahole.txt @ b827bfec55ca`。实测输出为 `size: 140536, members: 115`，并用于 `pvr_resume_count` 偏移 `140528` 的编译级门。

## 根因/排除项

030-032 把 `pm_probe` 状态插入与预编译 `fantgpu.o_shipped` 共享的 `struct dev_rsrc`。闭源对象按固定字段偏移消费这份结构，布局变化后在 `fixup_pcie_init` 处解引用错误，表现为 `cr2=0x2a9`。事故回包将根因明确记为：

```text
root_cause=030-032 inserted pm_probe state into dev_rsrc shared with precompiled fantgpu.o_shipped, shifting fixed field offsets
test_gap=no source or compiled ABI invariant existed for structures shared with shipped objects
```

`probe_executed=no` 排除了“探针运行时调用导致 Oops”这一说法；静态补丁测试、双构建字节一致和 030 链重放也没有覆盖共享闭源对象的结构布局不变量，因此不能把这些 PASS 当作 ABI 安全证明。

## 修复或边界

030-033 恢复 i3 的 `dev_rsrc` 布局，把探针状态移到独立的 per-device devres 分配；`fantgpu.o_shipped` 保持不变。i5 首启的正常探测、Driver/Firmware/DRM 健康和 `fixup_pcie_init` Oops 未复现，支持这条 ABI 修正链，但不等于 R5 已定位或已修复。

后续常设门包括：共享结构源码逐字比较；编译后的 BTF/`pahole` 检查；`builder_shipped_abi=PASS`；`size=140536`、`members=115` 和 `pvr_resume_count` 偏移 `140528`。i3 原始 `pahole` dump 已无法再生，`b827bfec…` 是 i5 实物；154 行源码逐字比较属于分层证据，不替代 i3 运行时 dump，也不替代运行级验证。

## 后续门槛

以后凡触及闭源对象共享的结构，初审必须先通过源码等价和编译级 ABI 门，再允许安装或执行诊断接口。任一结构大小、成员数、关键偏移或逐字比较不符时，批次停在静态审查，不生成新的运行结论；历史命令不构成当前执行授权。

冻结条款继续原样适用：`R5=FAIL`、`禁止重跑`、`U1/U2`、`validation-results`、`签发`、`tag`；事故记录中的“接受”只表示历史证据裁定被记录，不表示发布或测试授权。

## 验证边界

这份记录能证明 i4 启动 Oops 与共享 `dev_rsrc` 布局变化的因果链，并能证明 030-033 的静态 ABI 门和 i5 首启结果。它不能恢复不存在的 i3 dump，不能把 `b827bfec55ca` 改写成 i3 与 i5 的共同 dump，也不能把 i5 首启健康改写成 R5 挂起已解决。

## 回退条件

若 ABI 门失败、再次出现同类启动 Oops，或候选结构无法证明与 shipped object 等价，应停止该诊断批次，保留失败证据，不执行探针或 `pm_test`，并按 `patches/030-033.meta.json#L54-L56` 的链基回退约束回到 `030-032` 之前的可重建链基；不能通过反向编辑 F0 伪造恢复。i4 构建继续视为作废，任何新候选须另行审查。
