#nullable enable
using UnityEngine;

namespace FxNet.UdpTest
{
    /// <summary>
    /// UDP 测试引导器。在任意场景加载前自动运行，根据命令行解析出的角色
    /// 创建常驻的网络管理对象（客户端或服务器），无需手工在场景中挂载组件。
    ///
    /// 这样：
    /// - 本地 Windows 客户端：默认创建带 GUI 的 UdpTestClient；
    /// - Linux dedicated server：默认创建 UdpEchoServer；
    /// - WSL headless 客户端：以 -role client -auto 覆盖，创建自动化 UdpTestClient。
    /// </summary>
    public static class UdpTestBootstrap
    {
        private static GameObject? _root;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        private static void Bootstrap()
        {
            if (_root != null) return;

            var cfg = UdpTestConfig.Parse();

            _root = new GameObject("FxNetUdpTest");
            Object.DontDestroyOnLoad(_root);

            Debug.Log($"[UdpTestBootstrap] 启动配置: {cfg}");

            if (cfg.Role == UdpRole.Server)
            {
                var server = _root.AddComponent<UdpEchoServer>();
                server.Configure(cfg);
            }
            else
            {
                var client = _root.AddComponent<UdpTestClient>();
                client.Configure(cfg);
            }
        }
    }
}
