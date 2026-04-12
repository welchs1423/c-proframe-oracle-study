#ifndef SERVER_HANDLERS_H
#define SERVER_HANDLERS_H

#include "protocol.h"

/* =========================================
   [server_handlers.h] 서버 측 요청 핸들러 선언
   각 함수는 클라이언트 소켓 fd와 요청 패킷을
   받아 DB를 처리한 후 응답 패킷을 send 한다.
   ========================================= */

/* 인증 관련 */
void srv_login(int fd, Packet *req);
void srv_create_account(int fd, Packet *req);
void srv_change_password(int fd, Packet *req);
void srv_delete_account(int fd, Packet *req);

/* 뱅킹 관련 */
void srv_check_balance(int fd, Packet *req);
void srv_deposit(int fd, Packet *req);
void srv_withdraw(int fd, Packet *req);
void srv_transfer(int fd, Packet *req);
void srv_view_history(int fd, Packet *req);
void srv_download_csv(int fd, Packet *req);
void srv_deposit_savings(int fd, Packet *req);

/* 관리자 관련 */
void srv_admin_view_accounts(int fd, Packet *req);
void srv_unlock_account(int fd, Packet *req);
void srv_grant_vip(int fd, Packet *req);
void srv_grant_credit(int fd, Packet *req);
void srv_pay_interest(int fd, Packet *req);
void srv_system_stats(int fd, Packet *req);

#endif /* SERVER_HANDLERS_H */
