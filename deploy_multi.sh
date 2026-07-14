#!/bin/bash
SERVER_IP="115.190.230.47"
SSH_OPTS="-o StrictHostKeyChecking=no -o BatchMode=yes"
WSL_PUBLISH_DIR="/mnt/d/fxnet/c#/publish/linux-x64"

echo "=== 停止旧服务器 ==="
ssh $SSH_OPTS root@$SERVER_IP "pkill -f FxNet.UdpServer || true"
sleep 2

echo "=== 上传新版本 ==="
ssh $SSH_OPTS root@$SERVER_IP "mkdir -p /root/udptest && rm -rf /root/udptest/*"
scp $SSH_OPTS "${WSL_PUBLISH_DIR}/FxNet.UdpServer" root@$SERVER_IP:/root/udptest/
ssh $SSH_OPTS root@$SERVER_IP "chmod +x /root/udptest/FxNet.UdpServer"

echo "=== 启动服务器 ==="
ssh $SSH_OPTS root@$SERVER_IP "cd /root/udptest && nohup ./FxNet.UdpServer > ttt.txt 2>&1 &"
sleep 3

PID=$(ssh $SSH_OPTS root@$SERVER_IP "pgrep -f FxNet.UdpServer | head -1")
if [ -n "$PID" ]; then
    echo "服务器已启动 (PID: $PID)"
else
    echo "服务器启动失败!"
    ssh $SSH_OPTS root@$SERVER_IP "cat /root/udptest/ttt.txt"
    exit 1
fi
