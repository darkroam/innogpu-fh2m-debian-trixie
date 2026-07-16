# Picom 合成器管理

## 当前状态

本机运行 Picom v13，源码基线为上游提交
`6d676824c457a933c52e3e92c5a1856466f90545`。安装位置为 `/usr/local/bin/picom`，当前已验证二进制
SHA-256 为：

```text
172e6640a28eac2deeb3ff16750d1fb773e2eaa0e6961c2781bffed19d9a13de
```

该哈希只记录本机当前构建证据；编译器或依赖变化后哈希可以变化，不能作为跨设备安装条件。

仓库 patch SHA-256 为
`9df67d95fe7677f0614f465915cbab06dd694ee4be407b55155e0be544fb21ae`；项目配置与当前运行配置
逐字一致，SHA-256 为
`b6728844e6857224c8731a8d31962ca91e3a965be393e7a1c0b53b9d3031cf18`。

## 原始问题

Innogpu GL 驱动没有在扩展字符串中列出 `GL_ARB_explicit_uniform_location`。上游 Picom 的 GL
初始化只检查扩展字符串，因此即使驱动的 GLSL 编译器实际支持 `layout(location = ...)`，GLX
backend 仍直接退出。

项目 patch 在扩展未声明时编译一个最小 vertex shader：

- 编译成功：记录 warning 并继续初始化 GLX；
- 编译失败：保持上游错误和退出行为；
- 扩展正常声明：不执行额外探测。

该补丁是受限兼容探测，不等于宣称驱动完整支持该扩展，也不掩盖真实 shader 编译失败。

## 已完成工作

Picom 处理不是一次配置替换，实际经过以下步骤：

1. 将旧的直接 `xcompmgr` 启动改为优先 Picom、缺失时才回退 xcompmgr，并增加单实例、XDG 配置
   路径和 cache 日志。该最终入口首次进入 dotfiles 提交 `7747c2d`。
2. 将临时使用的 `picom2.conf` 收敛为规范 `picom.conf`，启动命令同步使用新路径。
3. 适配 Picom v13 配置语法，删除旧 `animation-for-*` 选项，改用 `animations` preset。
4. 针对桌面发白依次隔离模糊和透明度：确认问题不是 st 源码，也不是单独由模糊造成；恢复 st，
   删除全局非活动窗口透明，并给 st 增加 100% opacity 规则。
5. 保留最终可用的 `dual_kawase` 模糊、圆角、淡入淡出和轻量动画，阴影保持关闭。
6. 确认上游 Picom 因驱动扩展字符串缺项拒绝 GLX，修改 Picom 为实际 shader 编译探测，完成构建
   并安装到 `/usr/local/bin/picom`。
7. 通过持续日志确认兼容 warning 后 Picom 继续运行，当前进程实际使用 patched 二进制和
   `~/.config/x11/picom.conf`。

## 当前配置

项目配置使用 `backend = "glx"`，保留 10px 圆角、`dual_kawase` 模糊、淡入淡出和轻量动画。
为避免整个桌面发白，全局 active/inactive opacity 均保持 100%，并显式让 st、mpv、OBS、Gimp
等窗口不透明。当前阴影关闭，模糊强度为 4。

会话启动只允许一个 Picom 进程，并写入用户 cache 日志。Picom 不存在时尝试启动 `xcompmgr`；
Picom 存在但 GLX 初始化失败时不自动掩盖错误，应先检查日志再手工回退。

## 验证状态与限制

- 干净上游 clone 已完成 patch 正向应用、反向识别和 Picom v13 全量构建。
- 项目配置通过临时构建的 Picom `--diagnostics` 解析；backend 为 GLX，renderer 为
  `Fantasy II-M`，`Accelerated: 1`。
- diagnostics 会额外试探 EGL，并报告缺少 `GL_EXT_EGL_image_storage`；当前配置不使用 EGL，
  该错误不影响 GLX 结论。
- 当前 Picom 进程和日志持续证明 shader 探测兼容分支生效。
- 当前设备尚未安装 `xcompmgr`，所以运行中的 Picom 不受影响，但立即回退前需先执行
  `sudo apt install xcompmgr`；新设备 Picom 依赖脚本会自动安装它。
- patch 固定到提交 `6d676824`，升级 Picom 时必须重新审查，不能跳过基线检查强行应用。

## 所有权边界

- 仓库维护 Picom patch、配置模板、构建脚本和独立会话片段。
- 不复制完整 `xprofile`，也不管理壁纸、输入法、代理、MPD 或其他桌面启动项。
- Picom 源码和 build 目录不进入本仓库。
- Picom 补丁不加入 innogpu DKMS deb；两者必须分别安装和回退。

安装、验证和恢复见 `../user/picom-install.md`，实施记录见
`../planning/picom-integration.md`。
