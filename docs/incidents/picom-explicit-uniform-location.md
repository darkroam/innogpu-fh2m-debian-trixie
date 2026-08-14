# Picom：GL_ARB_explicit_uniform_location 能力判断特例

## 现象

Innogpu GLSL 编译器可以编译 explicit uniform location，但运行时扩展字符串没有声明
`GL_ARB_explicit_uniform_location`，上游 Picom 在创建 GLX backend 前因此提前退出。

## 处理

`patches/picom/001-probe-explicit-uniform-location.patch` 在扩展缺失时编译最小 shader；编译成功
才继续，失败仍拒绝 GLX。能力判断放在 Picom 用户态，不通过修改预编译 Innogpu `.so`、增加 GPU
型号白名单或伪造扩展字符串解决。

## 维护边界

Picom 独立于 Innogpu 驱动 deb，升级上游 Picom 或更换驱动用户态后必须重新运行构建、单元和真实
GLX 验证。该特例不能推广为通用显示脚本逻辑。
