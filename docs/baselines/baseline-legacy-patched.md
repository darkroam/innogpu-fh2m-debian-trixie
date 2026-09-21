# 代一：legacy patched 阶段

版本范围：`3.3.3.42-patched-N`。这是按打包与维护方式划分的文档代际；本阶段内部
发生过载荷更换，不能把全部 patched 版本统称为原厂 fantgpu 血缘。

本目录 docs/baselines/ 与仓库根 baselines/（运行结果归档）同词异物、互不相关。
本页为 R17 第 2 轮草稿，待 qoder 初审、dsh 终审。代际事实基准为本机
`collab/R17-2026-09-19-文档优化迭代/report.md#L359-L380 @ b5816b7c5a41`（§21）。
当前安装和发布结论以 [status](../project/status.md) 为准；历史文档中的命令不构成执行授权。

## 身份与事实

| 项目 | 已核对事实 | 来源 |
| --- | --- | --- |
| 起止事件 | 2026-05-28 的 `0.5`/patched-5 记录至 2026-08-20 patched-27 目标提交；后者 tag 于次日创建 | §21；下表 Git 对象 |
| 载体 | 原包解包、补丁叠加，再打成独立版本 deb；p27 已属 Deepin `20250421190503-debug` 载荷 | p27 wrapper 与 coherent 构建器 |
| ABI 边界 | p27 的 DKMS、用户态和固件使用同一 Deepin 发布；早期原包来源与 p27 不能混为一谈 | coherent 构建器；[用户态混配事故](../incidents/patched-18-userspace-mix.md) |
| 代表 deb | `debs/innogpu-fh2m-trixie_3.3.3.42-patched-27.deb` | [本地包目录](../../debs/README.md)；下述实物核对 |
| 回滚物 | `debs/` 历史回退链，具体层级及 canonical 文件由 [recovery](../user/recovery.md) 管理 | 不以文档代际顺序替代回滚规程 |

| tag | 目标提交 | 目标提交日期（+08:00） | tag 创建日期（+08:00） |
| --- | --- | --- | --- |
| `0.5` | `2b4759a3e168` | 2026-05-28 14:53:11 | UNVERIFIED：轻量 tag 不保存独立创建时间 |
| `patched-27` | `f90e8bc431ee` | 2026-08-20 16:00:40 | 2026-08-21 09:37:22 |

`0.5` 的 README 明示官方 `innogpu-fh2m 3.3.3.42`，安装示例使用 patched-5。
`git merge-base --is-ancestor 0.5 patched-27` 的只读核对返回 `1`：
**0.5 非 patched-27 祖先**，两者仅归入同一文档阶段，不能画成直接继承链。

代表 deb 的完整 SHA-256：

```text
f384159751fed249263591ff46758bb32327d0048e0669747050b66db1e33c6a
```

**源表差异待 dsh 补记**：§21 原文为“原录，本机无实物，UNVERIFIED-实物”。
本轮 2026-09-21 只读核对发现上述 `debs/` 文件存在，`sha256sum` 与原录完整值一致；
`build/` 下没有同名文件不等于本机无实物。本页并列保留原裁定与新增核对事实，未改源表。

## 本代工作

在厂商包上积累 Debian 6.12 兼容、DP fbcon fallback、设备 connector 映射、fbdev mmap
及 DMA-BUF/vblank 等修正，由版本 wrapper 固定补丁选择。历史实验与失败候选留档，
不是每个 patched 版本都可安装或交付；补丁编号也不等同于 deb 版本或 Git tag。

p27 的 wrapper 调用 Deepin coherent 构建器，要求用户态与固件整套同源；
混配用户态和遗漏 shader 固件的失败过程分别保留在
[用户态混配](../incidents/patched-18-userspace-mix.md) 与
[固件遗漏](../incidents/patched-18-shader-firmware.md)。
[p20 旧载荷事故](../incidents/patched-20-legacy-helper-payload.md) 另说明了为何不能
只凭运行通过就复用历史包。后续整树 mtime 归一化及双构建记录见
[2026-08-20 release 审阅](../planning/release-review-2026-08-20.md)。

## 与上一代差异

这是本文档体系的第一代，没有上一文档代际可比较。相对原始厂商包，本代增加了
Debian 适配、设备修正、打包与回退约束；不能据此推断闭源载荷已经源码化或获准再分发。

下一代 [deepin 4.0.x 源码树迁移线](baseline-deepin-4.x.md) 以 p27 为对照迁移可维护源码，
改变源码和载荷的组织方式。p27 已使用 Deepin，因此该迁移不等于从另一厂商血缘切到 Deepin。

## 溯源与验证边界

- `README.md#L14-L22 @ 2b4759a3e168`：0.5 来源与 patched-5 示例。
- `scripts/build-patched27-foreign-dmabuf.sh#L11-L26 @ f90e8bc431ee`：p27 的开关、输出路径与构建器入口。
- `scripts/build-deepin-coherent.sh#L38-L60 @ f90e8bc431ee`：Deepin 输入身份断言。
- `debs/README.md#L50-L77 @ b7cdeb4aa8c1`：历史包哈希表、版本与 tag 边界。

本轮只读核对 Git 对象和现有包，没有重建、安装或补做历史运行验收；tag 存在、
文件存在及 SHA 一致分别证明身份和可追溯性，不证明当前设备验收或发布许可。
当前冻结不变：`OUTSIDE_COVERAGE`、`R5=FAIL`、禁止重跑、U1/U2 未执行、
validation-results 未签、签发冻结、fantgpu 5.0.0-iN 未打 tag。
