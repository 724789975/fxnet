#!/bin/bash
# ============================================================
# FxNet UDP 一键部署测试脚本（双模式）
# 用法: bash deploy_test.sh [客户端超时秒数]
# 前置: SSH 密钥认证已配置，WSL 中 dotnet.exe 可用
# ============================================================

# ======================== 配置 ========================
SERVER_IP="115.190.230.47"
SERVER_USER="root"
REMOTE_DIR="/root/udptest"
# WSL 路径（用于 scp/cp/bash 操作）
WSL_BASE="/mnt/d/fxnet/c#"
WSL_PUBLISH_DIR="${WSL_BASE}/publish/linux-x64"
WSL_LOG_DIR="${WSL_BASE}/logs"
# Windows 路径（用于 dotnet.exe 命令）
WIN_PUBLISH_DIR="D:\\fxnet\\c#\\publish\\linux-x64"
WIN_PROJECT_SERVER="D:\\fxnet\\c#\\FxNet.UdpServer\\FxNet.UdpServer.csproj"
WIN_PROJECT_CLIENT="D:\\fxnet\\c#\\FxNet.UdpClient\\FxNet.UdpClient.csproj"
SSH_HOST="${SERVER_USER}@${SERVER_IP}"
SSH_OPTS="-o StrictHostKeyChecking=no -o BatchMode=yes"

# 客户端运行时间（秒），可通过第一个参数覆盖
CLIENT_TIMEOUT="${1:-90}"

ssh_cmd() { ssh $SSH_OPTS $SSH_HOST "$1"; }
scp_to()  { scp $SSH_OPTS "$1" "${SSH_HOST}:$2"; }
scp_from(){ scp $SSH_OPTS "${SSH_HOST}:$1" "$2"; }

# ======================== 编译发布 ========================
build_server() {
    local extra_define="$1"  # 空=多线程, "SINGLE_THREAD"=单线程
    echo ""
    echo "=== 编译服务器 (linux-x64) ${extra_define:+[$extra_define]} ==="
    cd "$WSL_BASE"
    if [ -n "$extra_define" ]; then
        dotnet.exe publish "$WIN_PROJECT_SERVER" -c Release -r linux-x64 --self-contained true \
            -p:PublishSingleFile=true -p:PublishAot=false \
            -p:DefineConstants=SINGLE_THREAD \
            -o "$WIN_PUBLISH_DIR"
    else
        dotnet.exe publish "$WIN_PROJECT_SERVER" -c Release -r linux-x64 --self-contained true \
            -p:PublishSingleFile=true -p:PublishAot=false \
            -o "$WIN_PUBLISH_DIR"
    fi
    if [ $? -ne 0 ]; then echo "编译失败!"; return 1; fi
    echo "编译成功"
    return 0
}

build_client() {
    local extra_define="$1"
    echo ""
    echo "=== 编译客户端 ${extra_define:+[$extra_define]} ==="
    cd "$WSL_BASE"
    if [ -n "$extra_define" ]; then
        dotnet.exe build "$WIN_PROJECT_CLIENT" -p:DefineConstants=SINGLE_THREAD
    else
        dotnet.exe build "$WIN_PROJECT_CLIENT"
    fi
    if [ $? -ne 0 ]; then echo "客户端编译失败!"; return 1; fi
    echo "编译成功"
    return 0
}

# ======================== 部署 & 启动 ========================
deploy_and_start() {
    echo ""
    echo "=== 清空远程目录 & 上传 ==="
    ssh_cmd "mkdir -p $REMOTE_DIR"
    ssh_cmd "rm -rf ${REMOTE_DIR}/*"

    scp_to "${WSL_PUBLISH_DIR}/FxNet.UdpServer" "${REMOTE_DIR}/"
    ssh_cmd "chmod +x ${REMOTE_DIR}/FxNet.UdpServer"

    cat > /tmp/_udp_start.sh << 'STARTEOF'
#!/bin/bash
cd /root/udptest
nohup ./FxNet.UdpServer > ttt.txt 2>&1 &
STARTEOF
    scp_to /tmp/_udp_start.sh /tmp/_udp_start.sh
    ssh_cmd "chmod +x /tmp/_udp_start.sh"
    echo "上传完成"

    echo ""
    echo "=== 启动服务器 ==="
    ssh_cmd "pkill -f FxNet.UdpServer" || true
    sleep 1
    ssh_cmd "bash /tmp/_udp_start.sh" || true
    sleep 3

    PID=""
    for i in 1 2 3; do
        PID=$(ssh_cmd "pgrep -f FxNet.UdpServer | head -1")
        if [ -n "$PID" ]; then break; fi
        sleep 2
    done

    if [ -z "$PID" ]; then
        echo "服务器启动失败! 远程日志:"
        ssh_cmd "cat ${REMOTE_DIR}/ttt.txt" || echo "无日志"
        return 1
    fi
    echo "服务器已启动 (PID: $PID)"
    return 0
}

# ======================== 停止 & 收集日志 ========================
stop_and_collect() {
    local log_subdir="$1"
    echo ""
    echo "=== 停止服务器 & 拉取日志 ==="
    ssh_cmd "pkill -f FxNet.UdpServer" || true
    sleep 2

    mkdir -p "$log_subdir"
    scp_from "${REMOTE_DIR}/ttt.txt"       "${log_subdir}/ttt.txt"       || true
    scp_from "${REMOTE_DIR}/udp_server.txt" "${log_subdir}/udp_server.txt" || true

    local client_log="${WSL_BASE}/udp_client.txt"
    if [ -f "$client_log" ]; then
        cp "$client_log" "${log_subdir}/udp_client.txt"
    fi
    echo "日志: $log_subdir"
}

# ======================== 验证日志 ========================
verify_logs() {
    local log_subdir="$1"
    local mode_label="$2"
    echo ""
    echo "=== 验证 [${mode_label}] ==="

    local pass=true
    local client_log_file="${log_subdir}/udp_client.txt"

    if [ -f "$client_log_file" ]; then
        local sent recv pass_count fail_count
        sent=$(grep -c '\[发送\]' "$client_log_file" 2>/dev/null) || sent=0
        recv=$(grep -c '\[接收\]' "$client_log_file" 2>/dev/null) || recv=0
        pass_count=$(grep -c '\[PASS\]' "$client_log_file" 2>/dev/null) || pass_count=0
        fail_count=$(grep -c '\[FAIL\]' "$client_log_file" 2>/dev/null) || fail_count=0
        echo "  发送: ${sent} 接收: ${recv} PASS: ${pass_count} FAIL: ${fail_count}"

        if [ "$sent" -eq 0 ]; then echo "  [FAIL] 无发送记录"; pass=false; fi
        if [ "$recv" -eq 0 ]; then echo "  [FAIL] 无接收记录"; pass=false; fi
        if [ "$fail_count" -gt 0 ]; then
            echo "  [FAIL] ${fail_count} 项验证失败"
            pass=false
            grep '\[FAIL\]' "$client_log_file" | while read -r line; do echo "    $line"; done
        fi

        local verdict
        verdict=$(grep '验证结果' "$client_log_file" 2>/dev/null || echo "")
        if echo "$verdict" | grep -q "FAIL"; then
            echo "  [FAIL] 客户端判定: FAIL"; pass=false
        elif echo "$verdict" | grep -q "PASS"; then
            echo "  [OK] 客户端判定: PASS"
        fi
    else
        echo "  [FAIL] 客户端日志不存在"; pass=false
    fi

    local server_log_file="${log_subdir}/udp_server.txt"
    if [ -f "$server_log_file" ]; then
        local sv ms
        sv=$(grep -c '\[验证\]' "$server_log_file" 2>/dev/null) || sv=0
        ms=$(grep -c '模式=' "$server_log_file" 2>/dev/null) || ms=0
        echo "  服务器验证: ${sv} 条, 模式切换: ${ms} 条"
        if [ "$sv" -eq 0 ]; then echo "  [FAIL] 无验证记录"; pass=false; fi
    else
        echo "  [FAIL] 服务器日志不存在"; pass=false
    fi

    echo ""
    if $pass; then
        echo "=== [${mode_label}] PASS ==="
    else
        echo "=== [${mode_label}] FAIL ==="
    fi
    $pass && return 0 || return 1
}

# ============================================================
# 主流程
# ============================================================
echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  FxNet UDP 部署测试（双模式）            ║"
echo "╚══════════════════════════════════════════╝"
echo "客户端超时: ${CLIENT_TIMEOUT}s"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
MULTI_LOG="${WSL_LOG_DIR}/${TIMESTAMP}_multi"
SINGLE_LOG="${WSL_LOG_DIR}/${TIMESTAMP}_single"
MULTI_RESULT=1
SINGLE_RESULT=1

# ======================== 第一轮：多线程 ========================
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  第一轮：多线程模式"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if build_server "" && build_client "" && deploy_and_start; then
    echo ""
    echo "=== 运行客户端 [多线程] ==="
    cd "$WSL_BASE"
    dotnet.exe run --no-build --project "$WIN_PROJECT_CLIENT" -- "$CLIENT_TIMEOUT"
    stop_and_collect "$MULTI_LOG"
    verify_logs "$MULTI_LOG" "多线程"
    MULTI_RESULT=$?
else
    echo "多线程模式部署失败!"
fi

# ======================== 第二轮：单线程 ========================
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  第二轮：单线程模式 (SINGLE_THREAD)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if build_server "SINGLE_THREAD" && build_client "SINGLE_THREAD" && deploy_and_start; then
    echo ""
    echo "=== 运行客户端 [单线程] ==="
    cd "$WSL_BASE"
    dotnet.exe run --no-build --project "$WIN_PROJECT_CLIENT" -- "$CLIENT_TIMEOUT"
    stop_and_collect "$SINGLE_LOG"
    verify_logs "$SINGLE_LOG" "单线程"
    SINGLE_RESULT=$?
else
    echo "单线程模式部署失败!"
fi

# ======================== 汇总 ========================
echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  最终结果                                ║"
echo "╚══════════════════════════════════════════╝"
echo ""
echo "  多线程: $([ $MULTI_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "  单线程: $([ $SINGLE_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo ""
echo "  多线程日志: $MULTI_LOG"
echo "  单线程日志: $SINGLE_LOG"
echo ""

if [ $MULTI_RESULT -eq 0 ] && [ $SINGLE_RESULT -eq 0 ]; then
    echo "=== 全部通过 ==="
else
    echo "=== 存在失败 ==="
fi
