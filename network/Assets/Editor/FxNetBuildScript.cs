#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace FxNet.UdpTest.EditorTools
{
    /// <summary>
    /// FxNet UDP 测试自动化打包脚本。可在 Unity 编辑器菜单调用，
    /// 也可通过命令行 batchmode -executeMethod 调用（供 CI/部署脚本使用）。
    ///
    /// 命令行示例（Windows PowerShell）：
    ///   &amp; "C:\Program Files\Unity\Hub\Editor\2022.3.62f3c1\Editor\Unity.exe" `
    ///       -batchmode -quit -nographics `
    ///       -projectPath "D:\fxnet\unity\network" `
    ///       -executeMethod FxNet.UdpTest.EditorTools.FxNetBuildScript.BuildAll `
    ///       -logFile -
    ///
    /// 产物目录：D:\fxnet\unity\publish\
    ///   win-client\FxNetUdpClient.exe          本地交互式 Windows 客户端
    ///   linux-server\FxNetUdpServer            Linux dedicated server（同一二进制通过
    ///                                          -role client 亦可作 headless 客户端）
    /// </summary>
    public static class FxNetBuildScript
    {
        // 产物根目录（相对工程根 network/ 的上级 unity/publish）
        private static string PublishRoot =>
            Path.GetFullPath(Path.Combine(Application.dataPath, "..", "..", "publish"));

        private const string WinClientName = "FxNetUdpClient.exe";
        private const string LinuxServerName = "FxNetUdpServer";

        [MenuItem("FxNet/Build/Windows Client")]
        public static void BuildWindowsClient()
        {
            string outDir = Path.Combine(PublishRoot, "win-client");
            PrepareDir(outDir);

            EditorUserBuildSettings.standaloneBuildSubtarget = StandaloneBuildSubtarget.Player;

            var opts = new BuildPlayerOptions
            {
                scenes = EnabledScenes(),
                locationPathName = Path.Combine(outDir, WinClientName),
                target = BuildTarget.StandaloneWindows64,
                subtarget = (int)StandaloneBuildSubtarget.Player,
                options = BuildOptions.None,
            };

            RunBuild(opts, "Windows 客户端");
        }

        [MenuItem("FxNet/Build/Linux Dedicated Server")]
        public static void BuildLinuxServer()
        {
            string outDir = Path.Combine(PublishRoot, "linux-server");
            PrepareDir(outDir);

            // 关键：设为 Server 子目标 → 生成无图形的 dedicated server，并自动定义 UNITY_SERVER
            EditorUserBuildSettings.standaloneBuildSubtarget = StandaloneBuildSubtarget.Server;

            var opts = new BuildPlayerOptions
            {
                scenes = EnabledScenes(),
                locationPathName = Path.Combine(outDir, LinuxServerName),
                target = BuildTarget.StandaloneLinux64,
                subtarget = (int)StandaloneBuildSubtarget.Server,
                options = BuildOptions.None,
            };

            RunBuild(opts, "Linux Dedicated Server");
        }

        /// <summary>依次构建全部产物（供 CI/部署脚本一键调用）</summary>
        [MenuItem("FxNet/Build/Build All")]
        public static void BuildAll()
        {
            BuildLinuxServer();
            BuildWindowsClient();
        }

        // ======================== 内部工具 ========================

        private static string[] EnabledScenes()
        {
            var scenes = new List<string>();
            foreach (var s in EditorBuildSettings.scenes)
                if (s.enabled) scenes.Add(s.path);

            if (scenes.Count == 0)
            {
                // 兜底：使用工程内第一个场景，保证有可运行的启动场景
                string fallback = "Assets/Scenes/SampleScene.unity";
                if (File.Exists(Path.Combine(Application.dataPath, "..", fallback)))
                    scenes.Add(fallback);
            }
            return scenes.ToArray();
        }

        private static void PrepareDir(string dir)
        {
            if (Directory.Exists(dir))
                Directory.Delete(dir, true);
            Directory.CreateDirectory(dir);
        }

        private static void RunBuild(BuildPlayerOptions opts, string label)
        {
            Debug.Log($"[FxNetBuild] 开始构建 {label} → {opts.locationPathName}");
            BuildReport report = BuildPipeline.BuildPlayer(opts);
            BuildSummary summary = report.summary;

            if (summary.result == BuildResult.Succeeded)
            {
                Debug.Log($"[FxNetBuild] {label} 构建成功: {summary.totalSize} 字节, 用时 {summary.totalTime}");
            }
            else
            {
                Debug.LogError($"[FxNetBuild] {label} 构建失败: {summary.result} ({summary.totalErrors} errors)");
                // batchmode 下以非零退出码结束，供脚本判定
                if (Application.isBatchMode)
                    EditorApplication.Exit(1);
            }
        }
    }
}
#endif
