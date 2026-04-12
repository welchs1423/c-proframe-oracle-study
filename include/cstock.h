#ifndef CSTOCK_H
#define CSTOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"

/* =========================================
   세션 상태 구조체 (로그인 후 유지되는 데이터)
   ========================================= */
typedef struct {
    char acc_no[21];    /* 로그인한 계좌번호 (trim 완료) */
    char user_name[41]; /* 고객명 (trim 완료)           */
    char db_grade[12];  /* 등급: "NORMAL" or "VIP"      */
    long credit_limit;  /* 마이너스 통장 한도            */
} SessionState;

/* =========================================
   함수 프로토타입 선언
   ========================================= */

/* db_util.pc : DB 연결 및 공통 유틸리티 */
void connect_db(void);
void disconnect_db(void);
void format_comma(long amt, char *out_str);
void trim_string(char *str, int max_len);

/* auth.pc : 계좌 개설, 로그인, 비밀번호 변경, 계좌 해지 */
void create_account(void);
int  login(SessionState *sess);
void change_password(SessionState *sess);
int  delete_account(SessionState *sess);

/* banking.pc : 잔액 조회, 입출금, 이체, 내역, 예금 */
void check_balance(SessionState *sess);
void deposit(SessionState *sess);
void withdraw(SessionState *sess);
void transfer(SessionState *sess);
void view_history(SessionState *sess);
void download_history_csv(SessionState *sess);
void deposit_savings(SessionState *sess);

/* admin.pc : 관리자 대시보드 및 관리 기능 */
void admin_view_accounts(void);
void unlock_account(void);
void grant_vip(void);
void grant_credit(void);
void pay_interest(void);
void system_stats_dashboard(void);

#endif /* CSTOCK_H */
