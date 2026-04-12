#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
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
   - daemon() 기반 백그라운드 데몬 동작 (Phase 4)
   - SIGINT / SIGTERM Graceful Shutdown 지원 (Phase 4)
   ========================================= */

#define PID_FILE "cstock_server.pid"

static int server_fd = -1;

/* ── SIGCHLD: 자식 프로세스 종료 시 좀비 방지 ── */
static void handle_sigchld(int sig) {
    (void)sig;
    /* WNOHANG: 종료된 자식 프로세스만 수거, 블로킹 없음 */
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ── SIGINT(Ctrl+C) / SIGTERM(kill): Graceful Shutdown ──
   1) 리슨 소켓 닫기 (새 접속 차단)
   2) PID 파일 제거
   3) 종료 로그 기록 후 exit
   부모 프로세스는 DB 커넥션을 갖지 않음 — DB 종료는 자식 담당 */
static void handle_shutdown(int sig) {
    char msg[128];
    snprintf(msg, sizeof(msg),
             "========== C-Stock 서버 종료 (signal=%d) ==========", sig);
    write_log(LOG_INFO, msg);

    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    unlink(PID_FILE);
    _exit(0);   /* async-signal-safe: _exit 사용 */
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
        default: {
            char log_msg[128];
            snprintf(log_msg, sizeof(log_msg),
                     "[서버][PID:%d] 알 수 없는 패킷 타입: %d",
                     (int)getpid(), req->type);
            write_log(LOG_WARN, log_msg);
            break;
        }
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
            snprintf(log_msg, sizeof(log_msg),
                     "[서버][PID:%d] 클라이언트 연결 종료: %s:%d",
                     (int)my_pid, client_ip, ntohs(addr->sin_port));
            write_log(LOG_INFO, log_msg);
            break;
        }
        if (n != (int)sizeof(Packet)) {
            snprintf(log_msg, sizeof(log_msg),
                     "[서버][PID:%d] 패킷 크기 불일치 (수신 %d / 기대 %zu)",
                     (int)my_pid, n, sizeof(Packet));
            write_log(LOG_WARN, log_msg);
            break;
        }

        /* XOR 복호화 — 평문 패킷으로 변환 */
        decrypt_packet(&req);

        snprintf(log_msg, sizeof(log_msg),
                 "[서버][PID:%d] 패킷 수신 및 복호화 완료 (type=%d, acc=%.20s)",
                 (int)my_pid, req.type, req.acc_no);
        write_log(LOG_INFO, log_msg);

        if (req.type == PKT_DISCONNECT) {
            snprintf(log_msg, sizeof(log_msg),
                     "[서버][PID:%d] 클라이언트 정상 종료: %s",
                     (int)my_pid, client_ip);
            write_log(LOG_INFO, log_msg);
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

/* ────────────────────────────────────────
   daemonize(): 서버를 백그라운드 데몬으로 전환
   - glibc daemon(nochdir=1, noclose=0) 사용
     : 현재 작업 디렉토리 유지, stdin/stdout/stderr → /dev/null
   - 성공 시 PID 파일(cstock_server.pid) 작성
   ──────────────────────────────────────── */
static void daemonize(void) {
    char log_msg[128];

    /* 데몬화 전 안내 메시지 (터미널에 출력) */
    printf("[서버] 백그라운드 데몬으로 전환 중...\n");
    printf("[서버] PID 파일 : %s\n", PID_FILE);
    printf("[서버] 로그 파일: logs/system.log\n");
    printf("[서버] 종료 명령: kill $(cat %s)\n", PID_FILE);
    fflush(stdout);

    /* daemon(1, 0):
       - nochdir=1 : 현재 디렉토리 유지 (logs/ 상대경로 사용 위해 필수)
       - noclose=0 : stdin/stdout/stderr → /dev/null 리다이렉트
       내부적으로 fork → setsid 수행; 부모(원본 프로세스)는 exit */
    if (daemon(1, 0) < 0) {
        perror("[오류] daemon() 실패");
        exit(1);
    }

    /* ── 이 아래는 데몬(자식) 프로세스에서만 실행 ── */

    /* PID 파일 작성 */
    FILE *fp = fopen(PID_FILE, "w");
    if (fp) {
        fprintf(fp, "%d\n", (int)getpid());
        fclose(fp);
    } else {
        write_log(LOG_WARN, "[서버] PID 파일 작성 실패");
    }

    snprintf(log_msg, sizeof(log_msg),
             "========== C-Stock 서버 데몬 시작 (PID=%d, PORT=%d) ==========",
             (int)getpid(), SERVER_PORT);
    write_log(LOG_INFO, log_msg);
}

int main(void) {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd;
    int opt = 1;
    pid_t child_pid;
    char log_msg[128];

    /* ── 시그널 핸들러 등록 ── */
    signal(SIGINT,  handle_shutdown); /* Ctrl+C     */
    signal(SIGTERM, handle_shutdown); /* kill <PID> */
    signal(SIGPIPE, SIG_IGN);         /* broken pipe 무시 */
    signal(SIGCHLD, handle_sigchld);  /* 좀비 프로세스 자동 수거 */

    /* ── 시작 배너 (데몬화 전 터미널 출력) ── */
    printf("====================================================\n");
    printf("      C-Stock ATM Server v4.0 (Daemon + Graceful)\n");
    printf("====================================================\n");
    printf("[1/3] fork() 기반 멀티 클라이언트 모드\n");
    printf("      - 접속마다 자식 프로세스(Oracle 커넥션) 생성\n");
    printf("      - 패킷 XOR 암호화 통신 활성화\n");
    printf("      - 백그라운드 데몬 모드 (Phase 4)\n");

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

    /* ── 리슨 (backlog=128: 50 동시 접속 스트레스 테스트 대응) ── */
    if (listen(server_fd, 128) < 0) {
        perror("[오류] listen() 실패");
        close(server_fd);
        exit(1);
    }

    printf("[2/3] 소켓 바인드 완료.\n");
    printf("[3/3] 클라이언트 대기 중 — 포트 %d\n", SERVER_PORT);
    printf("====================================================\n\n");

    /* ── 데몬화: 터미널 반환, 백그라운드로 전환 ── */
    daemonize();

    /* ── Accept 루프 (부모 데몬 프로세스: 연결 수락 전담) ── */
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            /* EINTR: SIGCHLD 등 시그널에 의한 인터럽트 — 재시도 */
            if (errno == EINTR) continue;
            write_log(LOG_ERROR, "[서버] accept() 실패");
            continue;
        }

        child_pid = fork();

        if (child_pid < 0) {
            write_log(LOG_ERROR, "[서버] fork() 실패");
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
            /* ── 부모(데몬) 프로세스 ── */
            /* 부모는 client_fd 소유권을 자식에게 넘김 — 즉시 닫기 */
            close(client_fd);
            snprintf(log_msg, sizeof(log_msg),
                     "[서버][부모] PID %d 자식 프로세스 생성 완료", (int)child_pid);
            write_log(LOG_INFO, log_msg);
        }
    }

    close(server_fd);
    return 0;
}
