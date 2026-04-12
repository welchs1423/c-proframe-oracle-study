#ifndef PROTOCOL_H
#define PROTOCOL_H

/* =========================================
   [protocol.h] TCP/IP 클라이언트-서버 통신 프로토콜
   패킷 구조체 및 요청/응답 상수 정의
   ========================================= */

/* ── 서버 접속 설정 ── */
#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 9090

/* ── 요청 타입 (type 필드) ── */
#define PKT_LOGIN            1   /* 로그인 */
#define PKT_CREATE_ACCOUNT   2   /* 계좌 개설 */
#define PKT_CHECK_BALANCE    3   /* 잔액 조회 */
#define PKT_DEPOSIT          4   /* 입금 */
#define PKT_WITHDRAW         5   /* 출금 */
#define PKT_TRANSFER         6   /* 계좌 이체 */
#define PKT_VIEW_HISTORY     7   /* 거래 내역 조회 */
#define PKT_CHANGE_PASSWORD  8   /* 비밀번호 변경 */
#define PKT_DELETE_ACCOUNT   9   /* 계좌 해지 */
#define PKT_ADMIN_VIEW      10   /* 관리자: 전체 계좌 조회 */
#define PKT_UNLOCK_ACCOUNT  11   /* 관리자: 계좌 잠금 해제 */
#define PKT_GRANT_VIP       12   /* 관리자: VIP 등급 부여 */
#define PKT_GRANT_CREDIT    13   /* 관리자: 마이너스 한도 부여 */
#define PKT_PAY_INTEREST    14   /* 관리자: 일괄 이자 지급 */
#define PKT_SYSTEM_STATS    15   /* 관리자: 시스템 통계 */
#define PKT_DEPOSIT_SAVINGS 16   /* 정기 예금 가입 */
#define PKT_DOWNLOAD_CSV    17   /* 거래 내역 CSV 다운로드 */
#define PKT_DISCONNECT      99   /* 연결 종료 */

/* ── 응답 결과 코드 (result 필드) ── */
#define RES_OK        0   /* 성공 */
#define RES_FAIL      1   /* 실패 */
#define RES_LOCKED    2   /* 계좌 잠금 */
#define RES_NOT_FOUND 3   /* 계좌 없음 */
#define RES_DB_ERROR  4   /* DB 오류 */
#define RES_MORE_DATA 5   /* 다중 행 응답: 계속 */
#define RES_END_DATA  6   /* 다중 행 응답: 종료 */
#define RES_ADMIN    10   /* 관리자 로그인 성공 */

/* ── 통신 패킷 구조체 ──
   필드 용도:
     acc_no  : 주 계좌번호 (요청 시 로그인 계좌 또는 대상 계좌)
     extra   : 보조 데이터 — 비밀번호, 대상 계좌번호 등
     extra2  : 추가 보조 — 새 비밀번호, 확인용 등
     amount  : 금액 (입출금/이체/한도 등)
     data    : 응답 메시지 또는 '|' 구분 직렬화 데이터
     result  : 응답 코드 (RES_* 상수)
   ========================================= */
typedef struct {
    int  type;
    char acc_no[21];
    char extra[64];
    char extra2[64];
    long amount;
    char data[512];
    int  result;
} Packet;

/* ── 클라이언트 세션 상태 (로그인 후 유지) ── */
typedef struct {
    char acc_no[21];
    char user_name[41];
    char db_grade[12];
    long credit_limit;
    int  role;   /* 1 = 고객, 2 = 관리자 */
} ClientSession;

#endif /* PROTOCOL_H */
