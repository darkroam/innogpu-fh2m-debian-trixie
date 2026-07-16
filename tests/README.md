# Tests

该目录用于可重复运行的静态 fixture 和脚本回归测试。

显示 fixture 已净化并吸纳到 `xdisplay/`。运行：

```sh
tests/xdisplay/run-stage2-tests.sh
tests/xdisplay/run-stage2-watch-tests.sh
tests/xdisplay/run-stage4-regression-tests.sh
tests/xdisplay/run-install-tests.sh
```

当前结果分别为 12、4、11、4 项通过。前三组使用假 RandR/lid/DRM，安装器测试只写入 `/tmp`
下的临时 HOME；它们不会修改当前 `xprofile`、启动真实 watcher 或改变显示布局。迁移边界见
[`docs/planning/display-integration.md`](../docs/planning/display-integration.md)。

不得提交测试运行产生的锁文件、日志、runtime 目录或本机绝对路径。
