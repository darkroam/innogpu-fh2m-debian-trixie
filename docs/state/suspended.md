# 挂起项

以下事项未完成前不得在当前状态摘要中声明为已支持；恢复时须先补设计、测试和回退步骤。

本页只记录暂停事项和恢复条件，任务状态统一回到 [当前待办](current-work.md)，
运行结论统一回到 [status](../project/status.md)，不另建第二份任务进度表。

- **R5 已闭合（非待选）**：见 [R5 闭合](current-work.md#r5-闭合与-fantgpu-500-主线当前最高优先级)。早期 `OUTSIDE_COVERAGE` 与失败轮不重写。U1/U2/R9 保持 unverified，validation-results 字段不改，R1 保持 fail。tag `fantgpu-5.0.0-i12` 指向 `079b179`。未另行授权不得执行 pm_test/watchdog。

- [ ] xdisplay 适配器、manual marker、多外屏布局和自定义配置由 dotconfig 维护；本项目仅在 Innogpu
  设备环境变化时复核兼容环境变量和恢复钩子。
- [ ] 多外屏、无盖桌面、不同扩展坞和不同外屏的预接冷启动仍需要跨项目实机矩阵，结果分别回写
  dotconfig 的显示文档和本项目的设备兼容记录。
- [ ] framebuffer 保留策略只用于受控 A/B 诊断；不加入默认配置或设备钩子。
- [ ] logind 合盖策略是系统级独立决策，不随显示脚本自动修改。
- [ ] 隔离目录中的历史文件需满足稳定期和恢复演练条件后才能删除，不纳入本仓库。
