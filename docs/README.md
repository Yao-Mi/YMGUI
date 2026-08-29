# YMGUI 文档

文档按“当前事实、按需参考、历史归档”分层，避免用过期开发记录推断现状。

## 当前事实

- [当前状态](当前状态.md)：默认首读，是全部 DEVLOG 最终成果的去时间线汇总，列全已完成功能、验证基线和当前边界。
- [开发进程 V2](DEVLOG_V2.md)：V1 的当前续集，持续记录每轮开发的背景、改动、取舍与验证。
- [API 速查](API.md)：写应用或修改公开 API 时阅读。
- [架构](ARCHITECTURE.md)：修改库内部、渲染管线或跨层边界时阅读。
- [跨平台移植](CROSS_PLATFORM_PORTING.md)：处理 SDL、Windows、Android 或真实硬件时阅读。
- [代码风格](代码风格.md)：新增模块、设计 API 或整理目录时阅读。

## 辅助资料

- `shots/`：README 和 Demo 使用的截图。
- 各 `project_Demo/*/README.md`：单个完整应用的运行方式与应用内约束。

## 历史归档

`history/` 保留项目演进和旧阶段快照，不是当前事实来源。只有在追溯决策、调查回归或考古旧行为时才读取：

- [开发进程 V1（第 0～41 轮）](history/DEVLOG_V1.md)
- [旧项目快照](history/PROJECT.md)

历史归档冻结，日常变更续写到 `DEVLOG_V2.md`，不再追加 V1。
