# Baselines

三级：

- marker：短结论，跟踪。`runtime/latest-runtime-baseline.txt` 是 `4.0.0-i1` 的 22/9/4，不是现行 F 线，也不是 i12 验收。
- capture：带日期记录。已跟踪捕获不改字节。带日期的原始日志不进 git。
- pointer：`current.txt` 只指 `docs/project/status.md`，不造 PASS。

无版本的一行 PASS 不得引用为 i12。改当前机器前仍要本地运行时证据，例如 `desktop-hwgl/latest-ddx-test/summary.txt`。已跟踪集合以 `git ls-files baselines/` 为准。
