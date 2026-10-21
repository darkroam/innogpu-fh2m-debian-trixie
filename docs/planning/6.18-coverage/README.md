# P6.18 待机

挂起。不是 6.12 的替代行，也不升 iN。机器可读断点在
[system-kernel-matrix.json](../../patches/system-kernel-matrix.json) 的 `in_progress`：
`system=gentoo`，`kernel=6.18`。现行 Gentoo 仍是 `6.12.111-gentoo-x86_64`。

新封存对象出现前，不在 6.18 上加载模块。

## 硬阻断

6.18 x86_64 SMP 没有切回经典 `%gs:0x28` 的配置。`CONFIG_STACKPROTECTOR=y` 且 `CONFIG_SMP=y` 时，
金丝雀走 `-mstack-protector-guard-reg=gs` 与 `-mstack-protector-guard-symbol=__ref_stack_chk_guard`。
`fixed_percpu_data` 已不在。五个封存对象仍按固定偏移读金丝雀，合计 2284 处：

| 对象 | `%gs:0x28` |
|---|---:|
| `fantgpu.o_shipped` | 756 |
| `fantsrvkm.o_shipped` | 1330 |
| `fantvpu.o_shipped` | 76 |
| `fantsmmu.o_shipped` | 66 |
| `fantdma.o_shipped` | 56 |

`fantgpu.o_shipped` 的 `hwinfo_register+0x2a` 是 `mov %gs:0x28,%rax`。R57 在 6.18.54 上这条指令缺页。
开放源码胶水改不了这些字节。

## 四条否决

- 不把 `CONFIG_STACKPROTECTOR` 当成经典金丝雀开关。没有这个开关。非 SMP 的 global guard 也不是 `%gs:0x28`。
- 不改五个封存对象的字节。
- 不把 6.18 的 GS/percpu 改回 6.12 形态。那是架构分叉，不是本轨。
- 不把 `g-050-fb-helper-alloc` 打进 6.18。它是 6.12 胶水。

干净出口只剩厂商按 guard-symbol 重编这五个对象。现在没有。

## 胶水

`g-037` 到 `g-049` 对得上 6.18.54 头文件，只说明编译面当时能对上，不是验收，也不是现行 srcversion。
`g-049` 把五个封存对象拷进构建，所以编得过仍会链入金丝雀读。`g-050` 不打。

## 001–006

对当时的 6.18.54 树做了 `patch --dry-run --fuzz=0`，没有写入。

| 补丁 | 结果 | 边界 |
|---|---|---|
| 001 | rc=0，偏移 -6 | hunk 能套。换内核必须重验，不搬 6.12 验收 |
| 002 | rc=0，偏移 +3 | hunk 能套。机型 workaround，必须重验 |
| 003 | rc=1，reversed | 那棵树里已有 `HYGO0030`。不是上游已合入的证明，也不是验收 |
| 004 | rc=1，文件已搬 | 不能套。`sound/hda/controllers/intel.c` 在 Zhaoxin `0x3288` 与 Loongson 之间多了 HDMI ID |
| 005 | rc=1，上下文变了 | prompt 语义还在，fuzz=0 不能套。当时 `.config` 的 `=y` 来自 select |
| 006 | rc=0，偏移 +22 | hunk 能套。当时 `VMAP_PFN=y` 来自 i915 的 select，不是这条 prompt |

Debian 上「003/004 实测不必要」不能搬到 6.18。

## 执行门

恢复时先用干净 gentoo-sources，重算五个对象的 `%gs:0x28`。合计仍是 2284，或任一对象还在读，就停。
不安装，不 `modprobe`。004/005 若要打，先重写 hunk，再在干净树上 dry-run。003 只在干净树上重打。

## 验收口径

以下都成立才算通过。缺一项不算。

- 五个封存对象不再读 `%gs:0x28`。
- 模块放在 6.18 自己的目录，不覆盖 6.12 的 ko。
- 六项只记 6.18 自己的结果，不抄 6.12：两次干净编译 srcversion 相同、modeset、GL、VA-API、输入、告警。
- `hwinfo_g0m.bin` 仍缺就不升格。

## 误装回滚

本段没有新 ko。若把 6.18 模块装进了磁盘：删掉那份 extra ko，跑 `depmod`，启动回现有 6.12。
不改 GRUB 默认项。

## 版本

iN 保持 i12。6.18 不进 `current`，也不改 6.12 两行的构建身份。
