# local-extractor 使用说明（project-tools 的功能）

> 本地载荷提取与校验工具是 **project-tools 制品的一个功能**，不是独立制品/门禁。本说明只描述
> 该功能的用法；许可边界见 [docs/project/licensing.md](../project/licensing.md)（唯一权威文档）。
>
> 该功能**只完成本地载荷提取与校验**：它**不包含** drivers/ 源码、vendor 载荷、deb 生成包或
> 任何第三方二进制，也**不是完整驱动发布方案**——离开用户本地取得的原包，无法重建或安装驱动。

## 本功能包含

本页命令与默认清单仅针对 Deepin 202504。fantgpu 线使用独立
[`binary-manifest-fantgpu.json`](../../binary-manifest-fantgpu.json) 和
[O_stage 物化方案](../design/o-stage-integration-plan.md)，不得直接套用本页默认参数。

- 主线已删除 `tools/extract-vendor-binaries.sh`、`tools/internal/generate-binary-manifest.py`、
  `tools/internal/validate-binary-manifest.py` 和 `binary-manifest.json`。
  这些 O 线输入在 tag `innogpu-4x-frozen`，不在主线复用。
- `vendor-binary` 是来源分类，不是许可证。F 线清单是 `binary-manifest-fantgpu.json`。
- 文档：本说明、`docs/project/dependencies.md`（原包身份与 SHA-256）、`THIRD_PARTY_NOTICES.md`；
- 许可证：`LICENSE`（本项目原创层 GPL-3.0-or-later）、`LICENSES/`（标准条款副本，含上游 MIT）。

## 前提：自行取得原包

本项目不托管、不镜像、不自动下载原包。请**从第三方**（如 Deepin 官方渠道）自行取得：

- 包名：`innogpu-fh2m`；版本：`20250421190503-debug`；
- 完整 SHA-256：`b5a70e7854db6e199d208ff31296ff637f59b5731d31e8123f95c39009f6f5b2`
  （权威来源页面 URL 以 `docs/project/dependencies.md` 为准，当前待外部核实）。

## 本地提取与校验

主线构建器对 O 分支只打印「O 线仅从 deepin-4.0.2-i3 tag 检出构建」后退出。
不要在主线上调用已删除的提取器。

## 限制与责任

- 提取产物只供**本地**构建、安装与回退使用；未经对应权利方授权不得再次公开分发。
- 本功能不授予任何第三方内容的许可证；第三方条款以权利方文件为准（见 `THIRD_PARTY_NOTICES.md`）。
- 主线不含 `drivers/`。O 线源码在 tag `innogpu-4x-frozen`。`driver-source` 在主线上为空清单，状态仍是 BLOCKED，不是完整驱动。
