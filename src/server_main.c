#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include "../include/protocol.h"
#include "../include/server_handlers.h"
#include "../include/cstock.h"

/* =========================================
   [server_main.c] C-Stock ATM TCP/IP 서버
   - Oracle DB와 직접 연결 (자식 프로세스별 독립 커넥션)
   - 클라이언트 접속마다 fork() → 자식 프로세스가 전담 처리
   - SIGCHLD 핸들러로 좀비 프로세스 방지
   - 모든 패킷 송수신에 XOR 암호화/복호화 적용
   ========================================= */

static int server_fd = -1;

/* ── SIGCHLD: 자식 프로세스 종료 시 좀비 방지 ── */
static void handle_sigchld(int sig) {
    (void)sig;
    /* WNOHANG: 종료된 자식 프로세스만 수거, 블로킹 없음 */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ── SIGINT(Ctrl+C): 부모 프로세스 graceful shutdown ── */
static void handle_sigint(int sig) {
    (void)sig;
    printf("\n[서버] 종료 신호 수신. 서버를 정지합니다...\n");
    if (server_fd >= 0) close(server_fd);
    /* 부모는 DB 커넥션 없음 — 자식만 connect_db/disconnect_db 담당 */
    write_log(LOG_INFO, "========== C-Stock 서버 종료 ==========");
    exit(0);
}

/* ── 패킷 타입 → 핸들러 디스패치 ── */
static void dispatch(int client_fd, Packet *req) {
    switch (req->type) {
        case PKT_LOGIN:            srv_login(client_fd, req);               break;
        case PKT_CREATE_ACCOUNT:   srv_create_account(client_fd, req);      break;
        case PKT_CHECK_BALANCE:    srv_check_balance(client_fd, req);       break;
        case PKT_DEPOSIT:          srv_deposit(client_fd, req);             break;
        case PKT_WITHDRAW:         srv_withdraw(client_fd, req);            break;
        case PKT_TRANSFER:         srv_transfer(client_fd, req);            break;
        case PKT_VIEW_HISTORY:     srv_view_history(client_fd, req);        break;
        case PKT_CHANGE_PASSWORD:  srv_change_password(client_fd, req);     break;
        case PKT_DELETE_ACCOUNT:   srv_delete_account(client_fd, req);      break;
        case PKT_DEPOSIT_SAVINGS:  srv_deposit_savings(client_fd, req);     break;
        case PKT_DOWNLOAD_CSV:     srv_download_csv(client_fd, req);        break;
        case PKT_ADMIN_VIEW:       srv_admin_view_accounts(client_fd, req); break;
        case PKT_UNLOCK_ACCOUNT:   srv_unlock_account(client_fd, req);      break;
        case PKT_GRANT_VIP:        srv_grant_vip(client_fd, req);           break;
        case PKT_GRANT_CREDIT:     srv_grant_credit(client_fd, req);        break;
        case PKT_PAY_INTEREST:     srv_pay_interest(client_fd, req);        break;
        case PKT_SYSTEM_STATS:     srv_system_stats(client_fd, req);        break;
        default:
            printf("[서버][PID:%d] 알 수 없는 패킷 타입: %d\n",
                   (int)getpid(), req->type);
            break;
    }
}

/* ────────────────────────────────────────
   클라이언트 1개 전담 처리 (자식 프로세스 내 실행)
   - 자신의 Oracle DB 커넥션으로 요청을 처리
   - 수신 패킷: decrypt 후 dispatch
   ──────────────────────────────────────── */
static void handle_client(int client_fd, struct sockaddr_in *addr) {
    Packet req;
    int n;
    char client_ip[INET_ADDRSTRLEN];
    char log_msg[256];
    pid_t my_pid = getpid();

    inet_ntop(AF_INET, &addr->sin_addr, client_ip, sizeof(client_ip));
    printf("[서버][PID:%d] 클라이언트 접속: %s:%d\n",
           (int)my_pid, client_ip, ntohs(addr->sin_port));

    snprintf(log_msg, sizeof(log_msg),
             "[서버][PID:%d] 클라이언트 접속: %s:%d",
             (int)my_pid, client_ip, ntohs(addr->sin_port));
    write_log(LOG_INFO, log_msg);

    /* 자식 프로세스 전용 Oracle DB 커넥션 수립 */
    connect_db();
    snprintf(log_msg, sizeof(log_msg),
             "[서버][PID:%d] Oracle DB 커넥션 수립 완료", (int)my_pid);
    write_log(LOG_INFO, log_msg);

    while (1) {
        /* 암호화된 패킷 수신 (MSG_WAITALL: 구조체 전체 보장) */
        n = recv(client_fd, &req, sizeof(Packet), MSG_WAITALL);
        if (n <= 0) {
            printf("[서버][PID:%d] 클라이언트 연결 종료: %s:%d\n",
                   (int)my_pid, client_ip, ntohs(addr->sin_port));
            break;
        }
        if (n != (int)sizeof(Packet)) {
            printf("[서버][PID:%d] 패킷 크기 불일치 (수신 %d / 기대 %zu)\n",
                   (int)my_pid, n, sizeof(Packet));
            break;
        }

        /* XOR 복호화 — 평문 패킷으로 변환 */
        decrypt_packet(&req);

        snprintf(log_msg, sizeof(log_msg),
                 "[서버][PID:%d] 패킷 수신 및 복호화 완료 (type=%d, acc=%.20s)",
                 (int)my_pid, req.type, req.acc_no);
        write_log(LOG_INFO, log_msg);

        if (req.type == PKT_DISCONNECT) {
            printf("[서버][PID:%d] 클라이언트 정상 종료: %s\n",
                   (int)my_pid, client_ip);
            break;
        }

        dispatch(client_fd, &req);
    }

    close(client_fd);

    /* 자식 프로세스 전용 커넥션 해제 및 종료 */
    disconnect_db();
    snprintf(log_msg, sizeof(log_msg),
             "[서버][PID:%d] Oracle DB 커넥션 해제 및 세션 종료", (int)my_pid);
    write_log(LOG_INFO, log_msg);
    exit(0);
}

int main(void) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    int opt = 1;
    pid_t child_pid;

    /* ── 시그널 핸들러 등록 ── */
    signal(SIGINT,  handle_sigint);
    signal(SIGPIPE, SIG_IGN);    /* broken pipe 무시 */
    signal(SIGCHLD, handle_sigchld); /* 좀비 프로세스 자동 수거 */

    /* ── 로그 초기화 ── */
    write_log(LOG_INFO, "========== C-Stock 서버 시작 (fork 멀티프로세스) ==========");
    printf("====================================================\n");
    printf("      C-Stock ATM Server v3.0 (Multi-Process)\n");
    printf("====================================================\n");
    printf("[1/3] fork() 기반 멀티 클라이언트 모드\n");
    printf("      - 접속마다 자식 프로세스(Oracle 커넥션) 생성\n");
    printf("      - 패킷 XOR 암호화 통신 활성화\n");

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

    /* ── 리슨 (backlog=10: 동시 대기 클라이언트 허용 수) ── */
    if (listen(server_fd, 10) < 0) {
        perror("[오류] listen() 실패");
        close(server_fd);
        exit(1);
    }

    printf("[2/3] 소켓 바인드 완료.\n");
    printf("[3/3] 클라이언트 대기 중 — 포트 %d\n", SERVER_PORT);
    printf("====================================================\n");
    printf("  Ctrl+C 로 서버를 종료할 수 있습니다.\n");
    printf("====================================================\n\n");

    /* ── Accept 루프 (부모 프로세스: 연결 수락 전담) ── */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            /* SIGCHLD에 의한 인터럽트인 경우 재시도 */
            perror("[오류] accept() 실패");
            continue;
        }

        child_pid = fork();

        if (child_pid < 0) {
            perror("[오류] fork() 실패");
            close(client_fd);
            continue;
        }

        if (child_pid == 0) {
            /* ── 자식 프로세스 ── */
            /* 자식은 서버 소켓 불필요 — 즉시 닫기 */
            close(server_fd);
            handle_client(client_fd, &client_addr);
            /* handle_client() 내부에서 exit(0) 호출 — 여기 도달 안 함 */
        } else {
            /* ── 부모 프로세스 ── */
            /* 부모는 client_fd 소유권을 자식에게 넘김 — 즉시 닫기 */
            close(client_fd);
            printf("[서버][부모] PID %d 자식 프로세스 생성 완료\n", (int)child_pid);
        }
    }

    close(server_fd);
    return 0;
}
