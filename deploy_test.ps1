# ============================================================
# FxNet UDP 一键部署测试脚本 (PowerShell 入口)
# 用法: .\deploy_test.ps1 [客户端运行秒数，默认60]
# 通过 WSL 执行实际的 bash 脚本
# ============================================================
param(
    [int]$ClientTimeout = 60
)
wsl -d Ubuntu-24.04 -- bash -c "cd '/mnt/d/fxnet/c#' && sed -i 's/\r`$//' deploy_test.sh && bash deploy_test.sh $ClientTimeout"
