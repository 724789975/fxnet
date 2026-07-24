# Changelog

本文件记录 FxNet 包的所有重要变更。

格式遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.0.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [1.1.0] - 2026-07-25

### Fixed
- UDP 断线感知：ACK 超时等连接异常时，`UdpConnector.OnError` 现会向上层推送 Error + Close 事件，驱动 `Session.OnClose` 回调。修复此前 UDP 掉线后底层 socket 已关闭但上层永不感知的「假死」问题，使自动重连得以触发（对齐 C++ `UDPConnectorIOErrorOperation` 行为）。

### Added
- Samples/UDP Echo Test：新增「阶段9 断线感知与自动重连」测试场景，复现 ACK 超时触发 OnClose 并验证重连后收发恢复。

## [1.0.0] - 2026-07-16

### Added
- 首次以 UPM 包形式发布。
- Runtime：TCP/UDP 连接与监听、可靠 UDP、滑动窗口、消息队列、多线程 IO。
- Samples：UDP Echo Test 示例（服务器 / 客户端 / 命令行配置 / 自动化测试引导器）。
