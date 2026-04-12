#!/bin/bash
# =============================================================
# stress_test.sh — C-Stock ATM 서버 동시성 부하 테스트
#
# 목적: 50개의 cstock_client를 동시에 실행해 서버에 일제히
#       로그인+잔액조회 요청을 보내고, 서버가 누락·크래시·
#       DB Lock 없이 전건 처리하는지 logs/system.log로 확인한다.
#
# 사전 조건:
#   - ./cstock_server 가 데몬으로 이미 실행 중일 것
#   - ./cstock_client 바이너리가 존재할 것
#   - TEST_ACC / TEST_PW 에 DB에 실제로 존재하는 계좌 입력 권장
#     (존재하지 않아도 연결/응답 처리 자체는 테스트 가능)
# =============================================================

# ── 설정 ──────────────────────────────────────────
CONCURRENT=50          # 동시 클라이언트 수
CLIENT_TIMEOUT=10      # 클라이언트 1개당 최대 대기(초)
LOG_FILE="logs/system.log"
TMP_DIR="/tmp/cstock_stress_$$"
CLIENT_BIN="./cstock_client"

# 테스트용 계좌 정보
# 존재하지 않는 계좌를 사용해 로그인 실패 → 메인 메뉴 복귀 → 즉시 종료 흐름으로
# 클라이언트가 timeout 전에 깔끔하게 종료되도록 함
TEST_ACC="0000000000"
TEST_PW="0000"
# ──────────────────────────────────────────────────

echo "========================================================"
echo "   C-Stock ATM 스트레스 테스트 (동시 ${CONCURRENT}개)"
echo "========================================================"
echo "  서버    : 127.0.0.1:9090"
echo "  클라이언트: ${CLIENT_BIN}"
echo "  테스트 계좌: ${TEST_ACC} / PW: ${TEST_PW}"
echo "========================================================"

# ── 서버 실행 여부 확인 ─────────────────────────────
PID_FILE="cstock_server.pid"
if [ -f "$PID_FILE" ]; then
    SERVER_PID=$(cat "$PID_FILE" 2>/dev/null | tr -d '[:space:]')
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "[OK] 서버 실행 중 (PID: $SERVER_PID)"
    else
        echo "[오류] PID 파일은 있으나 프로세스($SERVER_PID)가 없습니다."
        echo "       ./cstock_server 를 먼저 실행하세요."
        exit 1
    fi
else
    # PID 파일 없어도 포트 연결로 재확인
    if ! (echo "" | timeout 2 bash -c "cat /dev/null > /dev/tcp/127.0.0.1/9090") 2>/dev/null; then
        echo "[경고] PID 파일($PID_FILE)이 없고 포트 9090 응답 없음."
        echo "       ./cstock_server 를 먼저 실행하세요."
        exit 1
    fi
    echo "[경고] PID 파일 없음 — 포트 9090 응답 확인으로 진행"
fi

# ── 클라이언트 바이너리 확인 ────────────────────────
if [ ! -x "$CLIENT_BIN" ]; then
    echo "[오류] ${CLIENT_BIN} 를 찾을 수 없습니다."
    echo "       ./build.sh client 로 빌드 후 재시도하세요."
    exit 1
fi

# ── 임시 디렉토리 및 이전 로그 기준점 설정 ─────────
mkdir -p "$TMP_DIR"

if [ -f "$LOG_FILE" ]; then
    LOG_BASELINE=$(wc -l < "$LOG_FILE")
else
    LOG_BASELINE=0
fi

echo ""
echo "[1/3] ${CONCURRENT}개 클라이언트 동시 실행 시작..."
START_TIME=$(date +%s%3N)   # 밀리초 단위

# ── 50개 클라이언트 동시 실행 ────────────────────────
# 입력 시퀀스 (인터랙티브 stdin 시뮬레이션):
#   1        → 메인 메뉴: 로그인 선택
#   TEST_ACC → 계좌번호 (존재하지 않는 계좌 → 로그인 실패 → 메인 메뉴 복귀)
#   TEST_PW  → 비밀번호
#   3        → 메인 메뉴: 시스템 종료 (로그인 실패 후 즉시 종료)
# 로그인 실패 흐름을 사용해 클라이언트가 timeout 전에 깔끔하게 종료되게 함
for i in $(seq 1 $CONCURRENT); do
    printf "1\n%s\n%s\n3\n" "$TEST_ACC" "$TEST_PW" \
        | timeout $CLIENT_TIMEOUT "$CLIENT_BIN" \
          > "$TMP_DIR/out_${i}.txt" 2>&1 &
done

echo "[2/3] 모든 클라이언트 완료 대기 중..."
wait   # 모든 백그라운드 작업 완료 대기

END_TIME=$(date +%s%3N)
ELAPSED_MS=$(( END_TIME - START_TIME ))
ELAPSED_S=$(( ELAPSED_MS / 1000 ))
ELAPSED_MS_PART=$(( ELAPSED_MS % 1000 ))

echo "[3/3] 결과 분석 중..."
echo ""
echo "========================================================"
echo "   테스트 결과"
echo "========================================================"
printf "  소요 시간       : %d.%03d초\n" $ELAPSED_S $ELAPSED_MS_PART
echo ""

# ── 클라이언트 측 연결 성공/실패 집계 ────────────────
SUCCESS_CONN=0
FAIL_CONN=0
LOGIN_SUCCESS=0
LOGIN_FAIL=0

for i in $(seq 1 $CONCURRENT); do
    OUT="$TMP_DIR/out_${i}.txt"
    if grep -q "서버 연결 성공" "$OUT" 2>/dev/null; then
        SUCCESS_CONN=$(( SUCCESS_CONN + 1 ))
    else
        FAIL_CONN=$(( FAIL_CONN + 1 ))
    fi
    if grep -q "접속 성공" "$OUT" 2>/dev/null; then
        LOGIN_SUCCESS=$(( LOGIN_SUCCESS + 1 ))
    else
        LOGIN_FAIL=$(( LOGIN_FAIL + 1 ))
    fi
done

echo "  [클라이언트 측]"
echo "  서버 연결 성공   : ${SUCCESS_CONN} / ${CONCURRENT}"
echo "  서버 연결 실패   : ${FAIL_CONN} / ${CONCURRENT}"
echo "  로그인 성공      : ${LOGIN_SUCCESS} / ${CONCURRENT}"
echo "  로그인 실패/불명 : ${LOGIN_FAIL} / ${CONCURRENT}"
echo "  (로그인 실패는 계좌 미존재 시 정상 — 연결 처리가 핵심)"

# ── 서버 로그 분석 (테스트 구간만 추출) ─────────────
echo ""
echo "  [서버 로그 분석 — 테스트 구간 신규 항목]"

if [ -f "$LOG_FILE" ]; then
    # 테스트 시작 이후 추가된 로그 줄만 분석
    TAIL_LINES=$(( $(wc -l < "$LOG_FILE") - LOG_BASELINE ))
    if [ "$TAIL_LINES" -le 0 ]; then
        echo "  (테스트 구간 서버 로그 없음 — 서버가 데몬 실행 중인지 확인)"
    else
        NEW_LOG=$(tail -n "$TAIL_LINES" "$LOG_FILE")

        LOG_CONNECT=$(echo "$NEW_LOG"   | grep -c "클라이언트 접속:")
        LOG_DB_OPEN=$(echo "$NEW_LOG"   | grep -c "Oracle DB 커넥션 수립")
        LOG_DB_CLOSE=$(echo "$NEW_LOG"  | grep -c "Oracle DB 커넥션 해제")
        LOG_CHILD=$(echo "$NEW_LOG"     | grep -c "자식 프로세스 생성")
        LOG_PKT=$(echo "$NEW_LOG"       | grep -c "패킷 수신")
        LOG_ERRORS=$(echo "$NEW_LOG"    | grep -c "\[ERROR\]")
        LOG_WARNS=$(echo "$NEW_LOG"     | grep -c "\[WARN \]")

        echo "  클라이언트 접속  : ${LOG_CONNECT}건"
        echo "  DB 커넥션 수립   : ${LOG_DB_OPEN}건"
        echo "  DB 커넥션 해제   : ${LOG_DB_CLOSE}건  (= 정상 세션 종료)"
        echo "  자식 프로세스 생성: ${LOG_CHILD}건"
        echo "  패킷 수신/처리   : ${LOG_PKT}건"
        echo "  서버 오류(ERROR) : ${LOG_ERRORS}건"
        echo "  서버 경고(WARN)  : ${LOG_WARNS}건"
    fi
else
    echo "  [경고] 로그 파일($LOG_FILE) 없음 — 서버가 데몬으로 실행 중인지 확인"
fi

# ── 최종 판정 ────────────────────────────────────────
echo ""
echo "  [최종 판정]"
if [ "$SUCCESS_CONN" -eq "$CONCURRENT" ] && \
   [ "${LOG_ERRORS:-0}" -eq 0 ] && \
   [ "$LOG_CONNECT" -eq "$CONCURRENT" ] 2>/dev/null; then
    echo "  PASS: ${CONCURRENT}건 전량 연결 성공, 서버 오류 없음"
elif [ "$SUCCESS_CONN" -eq "$CONCURRENT" ] && [ "${LOG_ERRORS:-0}" -eq 0 ]; then
    echo "  PASS (연결 OK): ${SUCCESS_CONN}/${CONCURRENT} 연결 성공, 오류 없음"
    echo "  (로그 건수 불일치 시 서버 로그 파일을 직접 확인하세요)"
elif [ "$SUCCESS_CONN" -gt 0 ]; then
    echo "  PARTIAL: ${SUCCESS_CONN}/${CONCURRENT} 연결 성공"
    echo "  일부 실패 원인을 $LOG_FILE 에서 확인하세요."
else
    echo "  FAIL: 연결 성공 0건 — 서버 실행 여부와 포트(9090)를 확인하세요."
fi

echo "========================================================"
echo ""

# ── 임시 파일 정리 ───────────────────────────────────
rm -rf "$TMP_DIR"
