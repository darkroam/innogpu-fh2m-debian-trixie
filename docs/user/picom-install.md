# Picom 安装、验证与恢复

## 安装依赖

```sh
sudo scripts/install-picom-prereqs-debian.sh
```

## 准备固定源码

```sh
git clone https://github.com/yshui/picom "$HOME/src/picom"
git -C "$HOME/src/picom" checkout 6d676824c457a933c52e3e92c5a1856466f90545
```

不要直接在未知 Picom 版本上强行应用补丁。构建脚本会检查提交和工作树范围。

## 构建和安装

```sh
scripts/build-patched-picom.sh --source "$HOME/src/picom"
scripts/build-patched-picom.sh --source "$HOME/src/picom" --install
scripts/install-picom-user.sh
```

`--install` 只将已构建的 Picom 二进制安装到 `/usr/local/bin/picom`，首次覆盖前保存
`/usr/local/bin/picom.before-innogpu`。用户安装器将配置和会话片段安装到 XDG 用户目录，并在
`xprofile` 没有现有 Picom/xcompmgr 启动逻辑时追加带边界标记的 source 块。它不启动 Picom，
因此不需要重启，也不会中断当前 X11 会话。

## 验证

进入 X11 后执行：

```sh
picom --version
pgrep -a -x picom
tail -n 50 "${XDG_CACHE_HOME:-$HOME/.cache}/picom.log"
picom --diagnostics --config "${XDG_CONFIG_HOME:-$HOME/.config}/x11/picom.conf"
```

本设备预期出现一次以下 warning，随后继续运行：

```text
GL_ARB_explicit_uniform_location is not listed by the driver, but shader compilation succeeded; continuing.
```

还应实机检查圆角、模糊、动画和全屏窗口。桌面发白时先确认配置中没有全局 inactive opacity，
不要立即修改 st 或显卡驱动。

`--diagnostics` 会同时探测未选用的 EGL backend；本设备出现
`GL_EXT_EGL_image_storage extension not available` 不代表 GLX 失败。以最后的 `Backend: glx`、
`GL renderer: Fantasy II-M` 和 `Accelerated: 1` 为准。

## 恢复

当前 Picom 失败时，不需要重启：

```sh
sudo apt install xcompmgr
pkill -x picom || true
xcompmgr &
```

恢复安装前文件：

```sh
sudo cp -a /usr/local/bin/picom.before-innogpu /usr/local/bin/picom
cp -a "${XDG_CONFIG_HOME:-$HOME/.config}/x11/picom.conf.before-innogpu" \
  "${XDG_CONFIG_HOME:-$HOME/.config}/x11/picom.conf"
```

如果对应备份不存在，说明安装前没有该文件，不应执行恢复命令。Picom 问题与 DKMS 驱动包回退
分开处理。
