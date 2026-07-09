#!/bin/bash
# ============================================================
# FxNet UDP 一键部署测试脚本
# 用法: bash deploy_test.sh
# 前置: SSH 密钥认证已配置
# ============================================================

# ======================== 配置 ========================
SERVER_IP="115.190.230.47"
SERVER_USER="root"
REMOTE_DIR="/root/udptest"
PUBLISH_DIR="/mnt/f/share/fxnet/fxnet_c#/publish/linux-x64"
PROJECT_PATH="/mnt/f/share/fxnet/fxnet_c#/FxNet.UdpServer/FxNet.UdpServer.csproj"
LOCAL_LOG_DIR="/mnt/f/share/fxnet/fxnet_c#/logs"
CLIENT_PROJECT="/mnt/f/share/fxnet/fxnet_c#/FxNet.UdpClient/FxNet.UdpClient.csproj"
SSH_HOST="${SERVER_USER}@${SERVER_IP}"
SSH_OPTS="-o StrictHostKeyChecking=no -o BatchMode=yes"

ssh_cmd() { ssh $SSH_OPTS $SSH_HOST "$1"; }
scp_to()  { scp $SSH_OPTS "$1" "${SSH_HOST}:$2"; }
scp_from(){ scp $SSH_OPTS "${SSH_HOST}:$1" "$2"; }

# ======================== 1. 编译发布 ========================
echo ""
echo "=== 1/6 编译发布 (linux-x64) ==="
cd /mnt/f/share/fxnet/fxnet_c#
dotnet publish "$PROJECT_PATH" -c Release -r linux-x64 --self-contained true \
    -p:PublishSingleFile=true -p:PublishAot=false -o "$PUBLISH_DIR"
if [ $? -ne 0 ]; then echo "编译失败!"; exit 1; fi
echo "编译成功"

# ======================== 2. 清空远程目录 ========================
echo ""
echo "=== 2/6 清空远程目录 $REMOTE_DIR ==="
ssh_cmd "mkdir -p $REMOTE_DIR"
ssh_cmd "rm -rf ${REMOTE_DIR}/*"
echo "远程目录已清空"

# ======================== 3. 上传文件 ========================
echo ""
echo "=== 3/6 上传文件 ==="
scp_to "${PUBLISH_DIR}/FxNet.UdpServer" "${REMOTE_DIR}/"
ssh_cmd "chmod +x ${REMOTE_DIR}/FxNet.UdpServer"

# 创建并上传远程启动脚本
cat > /tmp/_udp_start.sh << 'STARTEOF'
#!/bin/bash
cd /root/udptest
nohup ./FxNet.UdpServer > ttt.txt 2>&1 &
STARTEOF
scp_to /tmp/_udp_start.sh /tmp/_udp_start.sh
ssh_cmd "chmod +x /tmp/_udp_start.sh"
echo "上传完成"

# ======================== 4. 启动服务器 ========================
echo ""
echo "=== 4/6 启动服务器 ==="
ssh_cmd "pkill -f FxNet.UdpServer" || true
sleep 1
ssh_cmd "bash /tmp/_udp_start.sh" || true
sleep 3

# 验证进程（重试3次）
PID=""
for i in 1 2 3; do
    PID=$(ssh_cmd "pgrep -f FxNet.UdpServer | head -1")
    if [ -n "$PID" ]; then break; fi
    sleep 2
done

if [ -z "$PID" ]; then
    echo "服务器启动失败! 远程日志:"
    ssh_cmd "cat ${REMOTE_DIR}/ttt.txt" || echo "无日志"
    exit 1
fi
echo "服务器已启动 (PID: $PID)"

# ======================== 5. 运行客户端测试 ========================
echo ""
echo "=== 5/6 运行客户端测试 (30s) ==="
cd /mnt/f/share/fxnet/fxnet_c#
dotnet run --project "$CLIENT_PROJECT"
echo "客户端测试完成"

# ======================== 6. 停止服务器 & 拉取日志 ========================
echo ""
echo "=== 6/6 停止服务器并拉取日志 ==="

ssh_cmd "pkill -f FxNet.UdpServer" || true
sleep 2
echo "服务器已停止"

# 创建本地日志目录（按时间戳）
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_SUBDIR="${LOCAL_LOG_DIR}/${TIMESTAMP}"
mkdir -p "$LOG_SUBDIR"

echo "拉取服务器日志..."
scp_from "${REMOTE_DIR}/ttt.txt"       "${LOG_SUBDIR}/ttt.txt"       || true
scp_from "${REMOTE_DIR}/udp_server.txt" "${LOG_SUBDIR}/udp_server.txt" || true

# 复制本地客户端日志
CLIENT_LOG="/mnt/f/share/fxnet/fxnet_c#/FxNet.UdpClient/udp_client.txt"
if [ -f "$CLIENT_LOG" ]; then
    cp "$CLIENT_LOG" "${LOG_SUBDIR}/udp_client.txt"
fi

echo ""
echo "日志已保存到: $LOG_SUBDIR"
echo "  - ttt.txt        (服务器控制台输出)"
echo "  - udp_server.txt (服务器业务日志)"
echo "  - udp_client.txt (客户端日志)"

# ======================== 7. 自动验证日志 ========================
echo ""
echo "=== 7/7 自动验证日志 ==="

PASS=true

# 验证客户端日志
CLIENT_LOG_FILE="${LOG_SUBDIR}/udp_client.txt"
if [ -f "$CLIENT_LOG_FILE" ]; then
    SENT=$(grep -c '\[发送\]' "$CLIENT_LOG_FILE" 2>/dev/null || echo 0)
    RECV=$(grep -c '\[接收\]' "$CLIENT_LOG_FILE" 2>/dev/null || echo 0)
    echo "  客户端 - 发送记录: ${SENT} 条, 接收记录: ${RECV} 条"
    if [ "$SENT" -eq 0 ]; then
        echo "  [FAIL] 客户端无发送记录"
        PASS=false
    fi
    if [ "$RECV" -eq 0 ]; then
        echo "  [FAIL] 客户端无接收记录"
        PASS=false
    fi
else
    echo "  [FAIL] 客户端日志文件不存在: $CLIENT_LOG_FILE"
    PASS=false
fi

# 验证服务器日志
SERVER_LOG_FILE="${LOG_SUBDIR}/udp_server.txt"
if [ -f "$SERVER_LOG_FILE" ]; then
    SERVER_RECV=$(grep -c '接收' "$SERVER_LOG_FILE" 2>/dev/null || echo 0)
    SERVER_SEND=$(grep -c '发送' "$SERVER_LOG_FILE" 2>/dev/null || echo 0)
    echo "  服务器 - 接收相关记录: ${SERVER_RECV} 条, 发送相关记录: ${SERVER_SEND} 条"
    if [ "$SERVER_RECV" -eq 0 ]; then
        echo "  [FAIL] 服务器无接收记录"
        PASS=false
    fi
    if [ "$SERVER_SEND" -eq 0 ]; then
        echo "  [FAIL] 服务器无发送记录"
        PASS=false
    fi
else
    echo "  [FAIL] 服务器日志文件不存在: $SERVER_LOG_FILE"
    PASS=false
fi

# 输出最终结果
echo ""
if $PASS; then
    echo "=== 验证结果: PASS ==="
else
    echo "=== 验证结果: FAIL ==="
fi
echo ""
echo "=== 全部完成 ==="
