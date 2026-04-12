#!/usr/bin/env bash
# ==============================================================
# build.sh — C-Stock ATM 빌드 스크립트
#
# 사용법:
#   ./build.sh           → 모놀리식 ATM (cstock_atm)
#   ./build.sh server    → TCP/IP 서버 (cstock_server)
#   ./build.sh client    → TCP/IP 클라이언트 (cstock_client)
#   ./build.sh all       → 서버 + 클라이언트 모두 빌드
#
# Pro*C 프리컴파일: oracle21c 컨테이너 내부 proc 사용
# GCC 컴파일/링크: 호스트 Oracle Instant Client 사용
#
# 사전 조건:
#   - Docker가 실행 중이어야 합니다.
#   - oracle21c 컨테이너가 기동 상태여야 합니다.
#     (docker ps | grep oracle21c)
# ==============================================================

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

MODE="${1:-atm}"

echo "======================================================"
echo "  C-Stock ATM 빌드 시작 (모드: ${MODE})"
echo "======================================================"

# oracle21c 컨테이너 실행 여부 확인
if ! docker ps --format '{{.Names}}' | grep -q '^oracle21c$'; then
    echo "[ERROR] oracle21c 컨테이너가 실행 중이 아닙니다."
    echo "        docker start oracle21c 명령으로 컨테이너를 먼저 기동하세요."
    exit 1
fi

echo "[1/2] 이전 빌드 산출물 정리 (make clean)..."
make clean

echo ""
echo "[2/2] 빌드 실행..."

case "$MODE" in
    server)
        echo "  → cstock_server 빌드 (TCP/IP 서버)"
        make server
        echo ""
        echo "======================================================"
        echo "  서버 빌드 완료!"
        echo "  실행 방법: ./cstock_server"
        echo "  (클라이언트 연결 전에 서버를 먼저 실행하세요)"
        echo "======================================================"
        ;;
    client)
        echo "  → cstock_client 빌드 (TCP/IP 클라이언트, Oracle 불필요)"
        make client
        echo ""
        echo "======================================================"
        echo "  클라이언트 빌드 완료!"
        echo "  실행 방법: ./cstock_client"
        echo "  (서버가 실행 중이어야 합니다)"
        echo "======================================================"
        ;;
    all)
        echo "  → cstock_server + cstock_client 빌드"
        make server
        make client
        echo ""
        echo "======================================================"
        echo "  전체 빌드 완료!"
        echo "  1. 서버 실행: ./cstock_server"
        echo "  2. 클라이언트 실행: ./cstock_client  (별도 터미널)"
        echo "======================================================"
        ;;
    *)
        echo "  → cstock_atm 빌드 (기존 모놀리식 ATM)"
        make
        echo ""
        echo "======================================================"
        echo "  빌드 완료! 실행: ./run.sh"
        echo "======================================================"
        ;;
esac
