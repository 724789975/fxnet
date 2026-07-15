# FxNet

纯 C# 实现的 TCP/UDP 网络库，面向 Unity。提供可靠 UDP、滑动窗口、消息队列与多线程 IO 等能力。

## 安装

在目标工程的 `Packages/manifest.json` 的 `dependencies` 中添加：

```json
"com.fxnet.network": "https://github.com/724789975/fxnet.git?path=/unity/network/Packages/com.fxnet.network"
```

或在 Package Manager 窗口中选择 **Add package from git URL** / **Add package from disk**（选中本包的 `package.json`）。

## 使用

```csharp
using FxNet;

// 通过 FxNetInterface 初始化并创建连接 / 监听
```

程序集名称为 `FxNet`。若你的脚本使用了自定义 asmdef，请在其 `references` 中加入 `FxNet`；
未使用 asmdef（即位于 Assembly-CSharp）时会自动引用。

## 示例

在 Package Manager 中选中本包 → **Samples** → 点击 **UDP Echo Test** 的 **Import**，
即可将 UDP 回声服务器 / 客户端示例导入到工程的 `Assets/Samples/` 下。

## 目录结构

```
com.fxnet.network/
├── package.json
├── Runtime/            运行时代码（FxNet 程序集）
│   ├── FxNet.asmdef
│   ├── FxNetInterface.cs
│   ├── Core/
│   ├── Dll/
│   └── IO/
└── Samples~/           示例（按需导入）
    └── UdpTest/
```
