# FxNet - C# UDP 可靠传输库

FxNet 是一个基于滑动窗口协议的 UDP 可靠传输库，实现了序号确认(ACK)、超时重传、快速重传、拥塞控制等机制。

## 目录结构

```
├── FxNet/              # 核心库
│   ├── Core/           # 核心逻辑（BufferContral、NetWorkStream等）
│   ├── Dll/            # 对外封装（Connector、FxNetApi）
│   ├── IO/             # IO模块（UdpConnector、IoModule等）
│   └── Util/           # 工具类
├── FxNet.UdpClient/    # UDP测试客户端
├── FxNet.UdpServer/    # UDP测试服务器
├── deploy_test.sh      # 一键部署测试脚本（双模式）
└── deploy_test.ps1     # Windows部署脚本
```

## 核心功能

- **滑动窗口可靠传输**：基于序号的确认和重传机制
- **拥塞控制**：类似 TCP AIMD 算法（慢启动、拥塞避免、乘法减小）
- **RTT估算**：Jacobian/Karels 算法，自动调整超时时间
- **空包保活**：无数据时按指数退避发送空包维持连接
- **文本协议流**：处理粘包/拆包问题，支持长度前缀消息

## 关键配置参数

### Socket选项（与C++端对齐）
| 参数 | 值 | 说明 |
|------|-----|------|
| ReceiveBufferSize | 256KB | 接收缓冲区大小 |
| SendBufferSize | 256KB | 发送缓冲区大小 |
| Ttl | 128 | IP生存时间 |

### 滑动窗口配置
| 参数 | 值 | 说明 |
|------|-----|------|
| WindowSize | 32 | 滑动窗口大小 |
| UdpSendFrequency | 0.016s | UDP发送频率 |
| MaxPkgSize | 1024 | 最大数据包大小 |

## 代码质量保障与测试执行报告

### 修复的关键问题

#### 1. SendMessage 数据丢弃（导致98.45%丢包）
**问题**：`SendMessage` 方法中 `sendBuff.PopData(sendSize)` 一次性弹出全部数据，但 `BufferContral.Send` 仅能将窗口容量内的数据入窗口，导致未入窗口数据被丢弃。

**修复**：新增 `TryMoveSendBuffToWindow` 私有方法，仅弹出实际入窗口的字节数，未入窗口数据保留在 sendBuff。

**文件**：[UdpConnector.cs](file:///d:/fxnet/c#/FxNet/IO/UdpConnector.cs)

#### 2. 空包导致负长度崩溃
**问题**：UDP保活包无载荷（size=0），`OnRecvOperator` 将其推入缓冲区，`TextWorkStream.PopData` 读取4字节长度头得到负数垃圾值，导致 `Array.Copy` 负长度异常。

**修复**：
- `OnRecvOperator.Execute` 增加 `size == 0` 判断，过滤空包
- `TextWorkStream.PopData` 增加 `pkgLen < 0 || pkgLen > 1MB` 校验

**文件**：[UdpConnector.cs](file:///d:/fxnet/c#/FxNet/IO/UdpConnector.cs), [NetWorkStream.cs](file:///d:/fxnet/c#/FxNet/Core/NetWorkStream.cs)

#### 3. Socket选项未配置
**问题**：C#端未设置 `SO_RCVBUF`、`SO_SNDBUF` 和 `IP_TTL`，与C++端不一致。

**修复**：在 `UdpConnector.Connect` 方法中设置 `ReceiveBufferSize = 256 * 1024`、`SendBufferSize = 256 * 1024`、`Ttl = 128`。

**文件**：[UdpConnector.cs](file:///d:/fxnet/c#/FxNet/IO/UdpConnector.cs)

#### 4. 单线程模式多客户端测试失败
**问题**：单线程模式下，`Connector.UdpConnect` 等待socket创建的循环仅调用 `Thread.Sleep()`，没有后台IO线程处理事件。

**修复**：在等待循环中添加 `#if SINGLE_THREAD` 条件编译，调用 `FxNetInterface.ProcSingleThread()` 处理IO事件。

**文件**：[Connector.cs](file:///d:/fxnet/c#/FxNet/Dll/Connector.cs)

### 测试结果

#### 多线程模式测试结果

| 阶段 | 结果 | 详情 |
|------|------|------|
| 阶段1：基础消息回显验证 | ✅ PASS | 英文/中文/JSON/特殊字符/空消息/Unicode等 |
| 阶段2：延迟测量 | ✅ PASS | 平均延迟 77.19ms (5/5成功) |
| 阶段3：批量发送吞吐量 | ✅ PASS | 590/590，丢包率 0.0% |
| 阶段3.5：收发顺序验证 | ✅ PASS | 2000条随机长度消息，顺序正确 |
| 阶段4：服务器命令交互 | ✅ PASS | CLIENT_COUNT/STATS/SWITCH_MODE/UNKNOWN_CMD |
| 阶段5：回显模式验证 | ✅ PASS | 大写/反转/原文三种模式 |
| 阶段6：大数据传输 | ✅ PASS | 1KB/8KB/10KB |
| 阶段7：多客户端并发 | ✅ PASS | 3个客户端全部正确回显 |
| 阶段8：错误处理 | ✅ PASS | 关闭后发送不崩溃 |

#### 单线程模式测试结果

| 阶段 | 结果 | 详情 |
|------|------|------|
| 阶段1：基础消息回显验证 | ✅ PASS | 所有消息正确回显 |
| 阶段2：延迟测量 | ✅ PASS | 平均延迟 77.47ms |
| 阶段3：批量发送吞吐量 | ✅ PASS | 590/590，丢包率 0.0% |
| 阶段3.5：收发顺序验证 | ✅ PASS | 2000条全部正确 |
| 阶段4：服务器命令交互 | ✅ PASS | 所有命令响应正确 |
| 阶段5：回显模式验证 | ✅ PASS | 大写/反转/原文模式 |
| 阶段6：大数据传输 | ✅ PASS | 1KB/8KB/10KB |
| 阶段7：多客户端并发 | ✅ PASS | 3个客户端全部正确回显 |
| 阶段8：错误处理 | ✅ PASS | 关闭后发送不崩溃 |

#### 关键指标对比

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 阶段3批量发送丢包率 | 92.9% | 0.0% |
| 阶段3.5顺序验证丢包率 | 98.45% (多线程) / 98.9% (单线程) | 0.0% |
| 多客户端并发测试 | FAIL (返回null) | PASS |
| 空包处理 | 崩溃 | 正常过滤 |

## 部署与测试

### 一键部署测试（WSL）

```bash
# 进入项目目录
cd /mnt/d/fxnet/c#

# 运行部署测试（默认超时90秒）
bash deploy_test.sh

# 指定客户端超时时间
bash deploy_test.sh 150
```

### 手动运行

```bash
# 编译服务器（多线程模式）
dotnet.exe publish FxNet.UdpServer/FxNet.UdpServer.csproj -c Release -r linux-x64 --self-contained true -p:PublishSingleFile=true -o publish/linux-x64

# 编译服务器（单线程模式）
dotnet.exe publish FxNet.UdpServer/FxNet.UdpServer.csproj -c Release -r linux-x64 --self-contained true -p:PublishSingleFile=true -p:DefineConstants=SINGLE_THREAD -o publish/linux-x64

# 上传到服务器
scp publish/linux-x64/FxNet.UdpServer root@server-ip:/root/udptest/

# 在服务器上启动
nohup ./FxNet.UdpServer > ttt.txt 2>&1 &

# 运行客户端（多线程模式）
dotnet.exe run --project FxNet.UdpClient/FxNet.UdpClient.csproj

# 运行客户端（单线程模式）
dotnet.exe run --project FxNet.UdpClient/FxNet.UdpClient.csproj -p:DefineConstants=SINGLE_THREAD
```

## 测试客户端阶段说明

1. **阶段1**：基础消息回显 - 测试各种数据类型和特殊字符
2. **阶段2**：延迟测量 - PING-PONG往返时间测量（5次）
3. **阶段3**：批量发送 - 590条消息吞吐量测试
4. **阶段3.5**：顺序验证 - 2000条随机长度消息的顺序正确性
5. **阶段4**：命令交互 - 测试服务器命令响应
6. **阶段5**：回显模式 - 测试大写/反转/原文三种回显模式
7. **阶段6**：大数据传输 - 1KB/8KB/10KB消息传输
8. **阶段7**：多客户端并发 - 同时创建3个客户端连接
9. **阶段8**：错误处理 - 测试关闭后发送的健壮性

## 技术文档

### UDP可靠传输原理

#### 滑动窗口协议
- 发送方维护发送窗口，仅发送窗口内的数据
- 接收方维护接收窗口，按序确认收到的数据
- ACK累积确认，支持快速重传

#### 拥塞控制
- **慢启动**：初始cwnd=1，每收到一个ACK加倍
- **拥塞避免**：cwnd达到ssthresh后线性增长
- **乘法减小**：检测到丢包时cwnd减半

#### RTT估算（Jacobian/Karels算法）
- SRTT = (1 - α) × SRTT + α × RTT
- RTTVAR = (1 - β) × RTTVAR + β × |RTT - SRTT|
- RTO = SRTT + max(G, 4 × RTTVAR)

#### 空包保活机制
- 无数据时按指数退避发送空包（3字节包头，0载荷）
- 防止NAT超时断开连接

### 代码规范

- UDP保活包（3-byte header, 0 payload）必须在 `OnRecvOperator` 中过滤
- `TextWorkStream` 必须验证包长度：`pkgLen > 0 && pkgLen ≤ 1MB`
- `SendMessage` 必须调用 `TryMoveSendBuffToWindow` 而非直接 `PopData`
- `BufferContral.Send()` 返回值必须检查，仅弹出实际入窗口的数据

## 已知限制

- 跨公网UDP传输存在天然丢包（约89%+），建议在内网或稳定网络环境使用
- 当前窗口大小为32，高延迟网络下建议增加到64
- 最大数据包大小1024字节，超过会被拆分

## 许可证

MIT License
