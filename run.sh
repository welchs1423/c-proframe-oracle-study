#!/usr/bin/env bash
# ==============================================================
# run.sh — C-Stock ATM 실행 스크립트
#
# oracle21c 컨테이너(DB 서버)가 실행 중인 상태에서
# 호스트(WSL2) 환경에서 cstock_atm 바이너리를 실행합니다.
#
# 사전 조건:
#   - oracle21c 컨테이너가 기동 상태여야 합니다.
#   - ./build.sh 를 먼저 실행하여 cstock_atm 이 빌드되어 있어야 합니다.
# ==============================================================

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

# oracle21c 컨테이너 실행 여부 확인
if ! docker ps --format '{{.Names}}' | grep -q '^oracle21c$'; then
    echo "[ERROR] oracle21c 컨테이너가 실행 중이 아닙니다."
    echo "        docker start oracle21c 명령으로 컨테이너를 먼저 기동하세요."
    exit 1
fi

# 바이너리 존재 여부 확인
if [ ! -f "./cstock_atm" ]; then
    echo "[ERROR] cstock_atm 바이너리가 없습니다. 먼저 ./build.sh 를 실행하세요."
    exit 1
fi

# Oracle Instant Client 라이브러리 경로 설정
export LD_LIBRARY_PATH=/opt/oracle/instantclient_21_13:$LD_LIBRARY_PATH

echo "======================================================"
echo "  C-Stock ATM 시스템 시작"
echo "======================================================"

./cstock_atm
