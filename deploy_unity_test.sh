#!/bin/bash
# ============================================================
# FxNet Unity UDP 一键部署测试脚本 (WSL bash)
# ------------------------------------------------------------
# 参考 c#/deploy_test.sh，适配 Unity 构建产物（目录形式，非单文件）。
#
# 流程:
#   1. (可选) 调用 build_unity.ps1 打包 Linux server + Win 客户端
#   2. scp -r 整个 linux-server 目录到远程，启动 dedicated server (-role server)
#   3. 将同一 Linux 产物复制到 WSL 原生路径（规避 /mnt/d noexec），
#      以 headless 客户端形式运行 (-role client -auto)，连接远程服务器
#   4. 停止服务器、拉取日志
#   5. 复用 deploy_test.sh 的校验逻辑：grep [发送]/[接收]/[PASS]/[FAIL]/验证结果/[验证]/模式=
#
# 用法:
#   bash deploy_unity_test.sh [客户端超时秒数] [--build]
#     --build   先调用 build_unity.ps1 重新打包
#
# 前置: SSH 密钥认证已配置；WSL 中可访问 /mnt/d；如需 --build 则 powershell.exe 可用
# ============================================================

# ======================== 配置 ========================
SERVER_IP="115.190.230.47"
SERVER_USER="root"
REMOTE_DIR="/root/udptest_unity"
LISTEN_PORT=9001

# WSL 路径
WSL_BASE="/mnt/d/fxnet/unity"
WSL_PUBLISH="${WSL_BASE}/publish"
WSL_LINUX_SERVER_DIR="${WSL_PUBLISH}/linux-server"     # Unity Linux 产物目录
WSL_LOG_DIR="${WSL_BASE}/logs"
# WSL 原生运行路径（避免 /mnt/d 的 noexec / 权限问题）
WSL_RUN_DIR="${HOME}/fxnet_unity_client"

# Windows 路径（用于 powershell.exe 调用打包脚本）
WIN_BUILD_PS1="D:\\fxnet\\unity\\build_unity.ps1"

SERVER_BIN="FxNetUdpServer"      # 与 FxNetBuildScript.LinuxServerName 一致
SSH_HOST="${SERVER_USER}@${SERVER_IP}"
SSH_OPTS="-o StrictHostKeyChecking=no -o BatchMode=yes"

# 参数解析
CLIENT_TIMEOUT=90
DO_BUILD=false
for arg in "$@"; do
    case "$arg" in
        --build) DO_BUILD=true ;;
        *[0-9]*) [[ "$arg" =~ ^[0-9]+$ ]] && CLIENT_TIMEOUT="$arg" ;;
    esac
done

ssh_cmd() { ssh $SSH_OPTS $SSH_HOST "$1"; }
scp_to()  { scp $SSH_OPTS "$1" "${SSH_HOST}:$2"; }
scp_to_r(){ scp $SSH_OPTS -r "$1" "${SSH_HOST}:$2"; }
scp_from(){ scp $SSH_OPTS "${SSH_HOST}:$1" "$2"; }

# ======================== 打包 ========================
build_unity() {
    echo ""
    echo "=== 调用 build_unity.ps1 打包 (Linux server + Win 客户端) ==="
    powershell.exe -ExecutionPolicy Bypass -File "$WIN_BUILD_PS1" -Target all
    local rc=$?
    if [ $rc -ne 0 ]; then echo "打包失败! (exit $rc)"; return 1; fi
    echo "打包成功"
    return 0
}

# ======================== 部署 & 启动服务器 ========================
deploy_and_start() {
    echo ""
    echo "=== 检查 Linux 产物 ==="
    if [ ! -f "${WSL_LINUX_SERVER_DIR}/${SERVER_BIN}" ]; then
        echo "未找到 ${WSL_LINUX_SERVER_DIR}/${SERVER_BIN}，请先执行打包 (--build)"
        return 1
    fi

    echo ""
    echo "=== 清空远程目录 & 上传整个产物目录 ==="
    ssh_cmd "mkdir -p $REMOTE_DIR"
    ssh_cmd "rm -rf ${REMOTE_DIR}/*"
    # Unity 产物是目录（含 _Data / UnityPlayer.so），需整目录上传
    scp_to_r "${WSL_LINUX_SERVER_DIR}/." "${REMOTE_DIR}/"
    ssh_cmd "chmod +x ${REMOTE_DIR}/${SERVER_BIN}"

    # 远程启动脚本（headless dedicated server）
    cat > /tmp/_udp_unity_start.sh << STARTEOF
#!/bin/bash
cd ${REMOTE_DIR}
nohup ./${SERVER_BIN} -batchmode -nographics -role server -port ${LISTEN_PORT} > ttt.txt 2>&1 &
STARTEOF
    scp_to /tmp/_udp_unity_start.sh /tmp/_udp_unity_start.sh
    ssh_cmd "chmod +x /tmp/_udp_unity_start.sh"
    echo "上传完成"

    echo ""
    echo "=== 启动服务器 ==="
    ssh_cmd "pkill -f ${SERVER_BIN}" || true
    for i in $(seq 1 10); do
        if ! ssh_cmd "ss -ulnp | grep -q :${LISTEN_PORT}"; then break; fi
        sleep 1
    done
    sleep 1
    ssh_cmd "bash /tmp/_udp_unity_start.sh" || true
    sleep 4

    local PID=""
    for i in 1 2 3 4; do
        PID=$(ssh_cmd "pgrep -f ${SERVER_BIN} | head -1")
        if [ -n "$PID" ]; then break; fi
        sleep 2
    done

    if [ -z "$PID" ]; then
        echo "服务器启动失败! 远程日志:"
        ssh_cmd "cat ${REMOTE_DIR}/ttt.txt" || echo "无日志"
        return 1
    fi

    if ! ssh_cmd "ss -ulnp | grep -q :${LISTEN_PORT}"; then
        echo "服务器进程运行中 (PID: $PID) 但端口 ${LISTEN_PORT} 未绑定!"
        ssh_cmd "cat ${REMOTE_DIR}/ttt.txt" || echo "无日志"
        return 1
    fi
    echo "服务器已启动 (PID: $PID, 端口 ${LISTEN_PORT} 已绑定)"
    return 0
}

# ======================== 运行本地 headless 客户端 (WSL) ========================
run_client() {
    echo ""
    echo "=== 准备 WSL 原生运行目录 (规避 /mnt/d noexec) ==="
    rm -rf "$WSL_RUN_DIR"
    mkdir -p "$WSL_RUN_DIR"
    cp -r "${WSL_LINUX_SERVER_DIR}/." "$WSL_RUN_DIR/"
    chmod +x "${WSL_RUN_DIR}/${SERVER_BIN}"

    echo ""
    echo "=== 运行 headless 客户端 [连接 ${SERVER_IP}:${LISTEN_PORT}, 超时 ${CLIENT_TIMEOUT}s] ==="
    cd "$WSL_RUN_DIR"
    # 同一 Linux 二进制以 -role client 运行；-auto 触发全部测试并以退出码结束
    ./${SERVER_BIN} -batchmode -nographics \
        -role client -auto \
        -serverip "${SERVER_IP}" -port "${LISTEN_PORT}" \
        -duration "${CLIENT_TIMEOUT}"
    local rc=$?
    echo "客户端退出码: $rc"
    return $rc
}

# ======================== 停止 & 收集日志 ========================
stop_and_collect() {
    local log_subdir="$1"
    echo ""
    echo "=== 停止服务器 & 拉取日志 ==="
    ssh_cmd "pkill -f ${SERVER_BIN}" || true
    sleep 2

    mkdir -p "$log_subdir"
    scp_from "${REMOTE_DIR}/ttt.txt"        "${log_subdir}/ttt.txt"        || true
    scp_from "${REMOTE_DIR}/udp_server.txt" "${log_subdir}/udp_server.txt" || true

    # 客户端日志写在 WSL 运行目录（Unity persistentDataPath 兜底为工作目录）
    for cand in "${WSL_RUN_DIR}/udp_client.txt" \
                "${WSL_RUN_DIR}/${SERVER_BIN}_Data/udp_client.txt"; do
        if [ -f "$cand" ]; then cp "$cand" "${log_subdir}/udp_client.txt"; break; fi
    done
    echo "日志: $log_subdir"
}

# ======================== 验证日志 ========================
verify_logs() {
    local log_subdir="$1"
    echo ""
    echo "=== 验证日志 ==="

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
    if $pass; then echo "=== PASS ==="; else echo "=== FAIL ==="; fi
    $pass && return 0 || return 1
}

# ============================================================
# 主流程
# ============================================================
echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  FxNet Unity UDP 部署测试                ║"
echo "╚══════════════════════════════════════════╝"
echo "  服务器: ${SERVER_IP}:${LISTEN_PORT}"
echo "  客户端超时: ${CLIENT_TIMEOUT}s"
echo "  重新打包: ${DO_BUILD}"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="${WSL_LOG_DIR}/${TIMESTAMP}_unity"
RESULT=1

if $DO_BUILD; then
    build_unity || { echo "打包失败，终止"; exit 1; }
fi

if deploy_and_start; then
    run_client || true
    stop_and_collect "$LOG_DIR"
    verify_logs "$LOG_DIR"
    RESULT=$?
else
    echo "部署失败!"
fi

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║  最终结果                                ║"
echo "╚══════════════════════════════════════════╝"
echo "  结果: $([ $RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "  日志: $LOG_DIR"
echo ""
[ $RESULT -eq 0 ] && echo "=== 全部通过 ===" || echo "=== 存在失败 ==="
exit $RESULT
