#!/usr/bin/env bash
# ==============================================================
# build.sh — C-Stock ATM 빌드 스크립트
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

echo "======================================================"
echo "  C-Stock ATM 빌드 시작"
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
echo "[2/2] 빌드 실행 (make)..."
make

echo ""
echo "======================================================"
echo "  빌드 완료! 실행: ./run.sh"
echo "======================================================"
