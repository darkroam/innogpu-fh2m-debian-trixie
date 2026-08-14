# 当前 TODO

## 文档与维护

- [ ] 为每次新候选包建立独立的 `docs/patches/` 说明和 `docs/incidents/` 验收记录。
- [ ] 将长期维护所需的脚本参数逐步收敛为可审查的配置，保持 `scripts/<name>` 兼容入口不变。
- [ ] release 发布前确认 `debs/` 中包与当前状态文档的版本、哈希和验证证据一致。

## 当前活动项

本轮显示代码吸纳、安装集成、fixture、测试包构建、当前 X11 只读验证和文档复核均已完成。
尚未实施的 manual marker 与单设备适配器不属于当前活动工作，统一记录在 `suspended.md`。

Picom patch、配置和安装流程已按 `picom-integration.md` 完成吸纳。升级上游 Picom 时需要重新
审查固定基线 patch。
