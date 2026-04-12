#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../include/protocol.h"
#include "../include/server_handlers.h"
#include "../include/cstock.h"

/* =========================================
   [server_main.c] C-Stock ATM TCP/IP 서버
   - Oracle DB와 직접 연결
   - 클라이언트 연결을 accept하여 요청 패킷
     처리 후 응답 패킷 반환
   - 단일 스레드 순차 처리 (연결당 1 클라이언트)
   ========================================= */

static int server_fd = -1;

/* SIGINT(Ctrl+C) 처리 — graceful shutdown */
static void handle_sigint(int sig) {
    (void)sig;
    printf("\n[서버] 종료 신호 수신. 서버를 정지합니다...\n");
    if (server_fd >= 0) close(server_fd);
    disconnect_db();
    write_log(LOG_INFO, "========== C-Stock 서버 종료 ==========");
    exit(0);
}

/* 패킷 타입 → 핸들러 디스패치 */
static void dispatch(int client_fd, Packet *req) {
    switch (req->type) {
        case PKT_LOGIN:            srv_login(client_fd, req);            break;
        case PKT_CREATE_ACCOUNT:   srv_create_account(client_fd, req);   break;
        case PKT_CHECK_BALANCE:    srv_check_balance(client_fd, req);    break;
        case PKT_DEPOSIT:          srv_deposit(client_fd, req);          break;
        case PKT_WITHDRAW:         srv_withdraw(client_fd, req);         break;
        case PKT_TRANSFER:         srv_transfer(client_fd, req);         break;
        case PKT_VIEW_HISTORY:     srv_view_history(client_fd, req);     break;
        case PKT_CHANGE_PASSWORD:  srv_change_password(client_fd, req);  break;
        case PKT_DELETE_ACCOUNT:   srv_delete_account(client_fd, req);   break;
        case PKT_DEPOSIT_SAVINGS:  srv_deposit_savings(client_fd, req);  break;
        case PKT_DOWNLOAD_CSV:     srv_download_csv(client_fd, req);     break;
        case PKT_ADMIN_VIEW:       srv_admin_view_accounts(client_fd, req); break;
        case PKT_UNLOCK_ACCOUNT:   srv_unlock_account(client_fd, req);   break;
        case PKT_GRANT_VIP:        srv_grant_vip(client_fd, req);        break;
        case PKT_GRANT_CREDIT:     srv_grant_credit(client_fd, req);     break;
        case PKT_PAY_INTEREST:     srv_pay_interest(client_fd, req);     break;
        case PKT_SYSTEM_STATS:     srv_system_stats(client_fd, req);     break;
        default:
            printf("[서버] 알 수 없는 패킷 타입: %d\n", req->type);
            break;
    }
}

/* 클라이언트 1개 처리 루프 */
static void handle_client(int client_fd, struct sockaddr_in *addr) {
    Packet req;
    int n;
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[서버] 클라이언트 접속: %s:%d\n", client_ip, ntohs(addr->sin_port));

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "[서버] 클라이언트 접속: %s:%d",
             client_ip, ntohs(addr->sin_port));
    write_log(LOG_INFO, log_msg);

    while (1) {
        /* 패킷 수신 (MSG_WAITALL: 구조체 전체 수신 보장) */
        n = recv(client_fd, &req, sizeof(Packet), MSG_WAITALL);
        if (n <= 0) {
            printf("[서버] 클라이언트 연결 종료: %s:%d\n", client_ip, ntohs(addr->sin_port));
            break;
        }
        if (n != (int)sizeof(Packet)) {
            printf("[서버] 패킷 크기 불일치 (수신 %d / 기대 %zu)\n", n, sizeof(Packet));
            break;
        }

        if (req.type == PKT_DISCONNECT) {
            printf("[서버] 클라이언트 정상 종료: %s\n", client_ip);
            break;
        }

        dispatch(client_fd, &req);
    }

    close(client_fd);
}

int main(void) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    int opt = 1;

    /* ── 시그널 핸들러 등록 ── */
    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN); /* broken pipe 무시 */

    /* ── 로그 / DB 초기화 ── */
    write_log(LOG_INFO, "========== C-Stock 서버 시작 ==========");
    printf("====================================================\n");
    printf("        C-Stock ATM Server v2.0 (TCP/IP)\n");
    printf("====================================================\n");
    printf("[1/3] Oracle DB 연결 중...\n");
    connect_db();

    /* ── TCP 소켓 생성 ── */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[오류] socket() 실패");
        exit(1);
    }

    /* SO_REUSEADDR: 서버 재시작 시 포트 즉시 재사용 */
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* ── 바인드 ── */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[오류] bind() 실패");
        close(server_fd);
        exit(1);
    }

    /* ── 리슨 ── */
    if (listen(server_fd, 5) < 0) {
        perror("[오류] listen() 실패");
        close(server_fd);
        exit(1);
    }

    printf("[2/3] 소켓 바인드 완료.\n");
    printf("[3/3] 클라이언트 대기 중 — 포트 %d\n", SERVER_PORT);
    printf("====================================================\n");
    printf("  Ctrl+C 로 서버를 종료할 수 있습니다.\n");
    printf("====================================================\n\n");

    /* ── Accept 루프 ── */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("[오류] accept() 실패");
            continue;
        }
        handle_client(client_fd, &client_addr);
    }

    close(server_fd);
    disconnect_db();
    return 0;
}
