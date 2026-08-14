# Picom Patch 001：运行时验证 explicit uniform location

## 目的

Innogpu GLSL 编译器支持 explicit uniform location，但驱动扩展字符串未声明
`GL_ARB_explicit_uniform_location`，导致 Picom 上游在创建 backend 前提前退出。

## 实现

- 代码：`patches/picom/001-probe-explicit-uniform-location.patch`。
- 所属项目：Picom 用户态，不进入 Innogpu DKMS deb。
- 行为：扩展缺失时编译最小 shader；编译失败仍拒绝 GLX，成功才继续。

## 验证与边界

Picom 构建、单元测试和真实 `DISPLAY=:0` GLX 启动已通过。该特例不能扩展为 GPU 型号白名单，
也不能通过修改 Innogpu 预编译 `.so` 伪造扩展能力。
