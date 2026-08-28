# local-extractor 使用说明（project-tools 的功能）

> 本地载荷提取与校验工具是 **project-tools 制品的一个功能**，不是独立制品/门禁。本说明只描述
> 该功能的用法；许可边界见 [docs/project/licensing.md](../project/licensing.md)（唯一权威文档）。
>
> 该功能**只完成本地载荷提取与校验**：它**不包含** drivers/ 源码、vendor 载荷、deb 生成包或
> 任何第三方二进制，也**不是完整驱动发布方案**——离开用户本地取得的原包，无法重建或安装驱动。

## 本功能包含

- 工具：`scripts/extract-vendor-binaries.sh`（本地提取，仅读取用户本地原包）、
  `tools/generate-binary-manifest.py`、`tools/validate-binary-manifest.py`；
- 清单：`binary-manifest.json`（来源分类与哈希；`vendor-binary` 是来源分类，不是许可证）；
- 文档：本说明、`docs/project/dependencies.md`（原包身份与 SHA-256）、`THIRD_PARTY_NOTICES.md`；
- 许可证：`LICENSE`（本项目原创层 GPL-3.0-or-later）、`LICENSES/`（标准条款副本，含上游 MIT）。

## 前提：自行取得原包

本项目不托管、不镜像、不自动下载原包。请**从第三方**（如 Deepin 官方渠道）自行取得：

- 包名：`innogpu-fh2m`；版本：`20250421190503-debug`；
- 完整 SHA-256：`b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`
  （权威来源页面 URL 以 `docs/project/dependencies.md` 为准，当前待外部核实）。

## 本地提取与校验

```sh
# 1. 校验 manifest schema（只读）
python3 tools/validate-binary-manifest.py

# 2. 提取载荷到本地 vendor/（校验原包 SHA-256，幂等，原子写）
INNOGPU_DEEPIN_DEB=/path/to/innogpu-fh2m_20250421190503-debug_amd64.deb \
  bash scripts/extract-vendor-binaries.sh

# 3. 只读检查模式（不写任何文件）
bash scripts/extract-vendor-binaries.sh --check-only
```

## 限制与责任

- 提取产物只供**本地**构建、安装与回退使用；未经对应权利方授权不得再次公开分发。
- 本功能不授予任何第三方内容的许可证；第三方条款以权利方文件为准（见 `THIRD_PARTY_NOTICES.md`）。
- 完整驱动构建需要 `drivers/` 源码树与构建脚本，二者不在本功能内；`driver-source` 制品排除
  confidential 与无许可文件后**不是完整驱动**，缺失内容须由用户按原声明从本地原包取得。
