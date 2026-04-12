#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../include/protocol.h"

/* =========================================
   [client_main.c] C-Stock ATM TCP/IP 클라이언트
   - Oracle 라이브러리 불필요 (순수 C)
   - 사용자 UI / 입력 처리
   - Packet을 서버로 send, 응답 recv 후 출력
   ========================================= */

static int server_fd = -1;
static ClientSession sess;   /* 로그인 후 세션 상태 */

/* ────────────────────────────────────────
   내부 유틸리티
   ──────────────────────────────────────── */

/* 숫자를 천 단위 쉼표 포맷으로 변환 (클라이언트 전용) */
static void fmt_comma(long amt, char *out) {
    if (amt < 0) {
        char pos[50];
        fmt_comma(-amt, pos);
        sprintf(out, "-%s", pos);
        return;
    }
    char tmp[50];
    sprintf(tmp, "%ld", amt);
    int len = strlen(tmp);
    int cc  = (len - 1) / 3;
    int oi  = len + cc;
    out[oi] = '\0';
    oi--;
    int cnt = 0;
    for (int i = len - 1; i >= 0; i--) {
        out[oi--] = tmp[i];
        cnt++;
        if (cnt % 3 == 0 && i != 0) out[oi--] = ',';
    }
}

/* 서버에 패킷 송신 (XOR 암호화 후 전송) */
static int send_pkt(Packet *p) {
    Packet enc = *p;
    encrypt_packet(&enc);
    return send(server_fd, &enc, sizeof(Packet), 0);
}

/* 서버로부터 패킷 수신 (수신 후 XOR 복호화) */
static int recv_pkt(Packet *p) {
    int n = recv(server_fd, p, sizeof(Packet), MSG_WAITALL);
    if (n > 0) decrypt_packet(p);
    return n;
}

/* 단순 요청 → 단일 응답 수신 후 메시지 출력 */
static Packet do_request(int type, const char *acc,
                          const char *ex1, const char *ex2, long amount) {
    Packet req, res;
    memset(&req, 0, sizeof(req));
    req.type   = type;
    req.amount = amount;
    if (acc) strncpy(req.acc_no, acc, 20);
    if (ex1) strncpy(req.extra,  ex1, 63);
    if (ex2) strncpy(req.extra2, ex2, 63);

    send_pkt(&req);
    memset(&res, 0, sizeof(res));
    recv_pkt(&res);
    return res;
}

/* stdin 버퍼 정리 */
static void flush_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ────────────────────────────────────────
   서버 연결 / 해제
   ──────────────────────────────────────── */
static void connect_server(void) {
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[오류] socket() 실패");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) {
        perror("[오류] inet_pton() 실패");
        exit(1);
    }

    if (connect(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[오류] 서버 연결 실패 (서버가 실행 중인지 확인하세요)");
        exit(1);
    }
    printf("[클라이언트] 서버 연결 성공 (%s:%d)\n", SERVER_IP, SERVER_PORT);
}

static void disconnect_server(void) {
    Packet req;
    memset(&req, 0, sizeof(req));
    req.type = PKT_DISCONNECT;
    send_pkt(&req);
    close(server_fd);
}

/* ────────────────────────────────────────
   로그인
   ──────────────────────────────────────── */
static int client_login(void) {
    char acc_no[21], passwd[64];

    printf("\n계좌번호 입력: ");
    scanf("%20s", acc_no);
    printf("비밀번호 입력: ");
    scanf("%63s", passwd);

    Packet req, res;
    memset(&req, 0, sizeof(req));
    req.type = PKT_LOGIN;
    strncpy(req.acc_no, acc_no, 20);
    strncpy(req.extra,  passwd, 63);
    send_pkt(&req);
    recv_pkt(&res);

    printf("%s\n", res.data);

    if (res.result == RES_OK || res.result == RES_ADMIN) {
        /* data = "user_name|grade|credit_limit" 파싱 */
        char user_name[41], grade[12];
        long credit_limit = 0;
        sscanf(res.data, "%40[^|]|%11[^|]|%ld", user_name, grade, &credit_limit);

        strncpy(sess.acc_no,    acc_no,    20);
        strncpy(sess.user_name, user_name, 40);
        strncpy(sess.db_grade,  grade,     11);
        sess.credit_limit = credit_limit;
        sess.role         = (res.result == RES_ADMIN) ? 2 : 1;

        printf("\n[환영합니다] %s님 접속 성공! (등급: %s)\n",
               sess.user_name, sess.db_grade);
        return sess.role;
    }
    return 0;
}

/* ────────────────────────────────────────
   계좌 개설
   ──────────────────────────────────────── */
static void client_create_account(void) {
    char acc_no[21], uname[64], pw[64];

    printf("\n--- [신규 계좌 개설] ---\n");
    printf("희망 계좌번호 입력 (예: 110-111-222): ");
    scanf("%20s", acc_no);
    printf("고객명 입력: ");
    scanf("%63s", uname);
    printf("초기 비밀번호(4자리): ");
    scanf("%63s", pw);

    Packet res = do_request(PKT_CREATE_ACCOUNT, acc_no, uname, pw, 0);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   잔액 조회
   ──────────────────────────────────────── */
static void client_check_balance(void) {
    Packet res = do_request(PKT_CHECK_BALANCE, sess.acc_no, NULL, NULL, 0);
    if (res.result != RES_OK) { printf("%s\n", res.data); return; }

    long balance = 0, savings = 0, credit = 0;
    sscanf(res.data, "%ld|%ld|%ld", &balance, &savings, &credit);

    char bal_str[50], sav_str[50], lim_str[50], avail_str[50];
    fmt_comma(balance, bal_str);
    fmt_comma(savings, sav_str);

    printf("\n현재 잔액: %s원 (등급: %s)\n", bal_str, sess.db_grade);
    printf("예금 잔액: %s원\n", sav_str);
    if (credit > 0) {
        fmt_comma(credit, lim_str);
        fmt_comma(balance + credit, avail_str);
        printf("마이너스 통장 한도: %s원 | 사용 가능 금액: %s원\n", lim_str, avail_str);
    }
}

/* ────────────────────────────────────────
   입금
   ──────────────────────────────────────── */
static void client_deposit(void) {
    long amount;
    printf("\n입금할 금액: ");
    scanf("%ld", &amount);

    Packet res = do_request(PKT_DEPOSIT, sess.acc_no, NULL, NULL, amount);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   출금
   ──────────────────────────────────────── */
static void client_withdraw(void) {
    long amount;
    printf("\n출금할 금액: ");
    scanf("%ld", &amount);

    Packet res = do_request(PKT_WITHDRAW, sess.acc_no, NULL, NULL, amount);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   계좌 이체
   extra  = 대상 계좌번호
   extra2 = 등급 (수수료 계산용)
   ──────────────────────────────────────── */
static void client_transfer(void) {
    char target[21];
    long amount;

    printf("\n대상 계좌번호: ");
    scanf("%20s", target);
    printf("이체할 금액: ");
    scanf("%ld", &amount);

    Packet req;
    memset(&req, 0, sizeof(req));
    req.type   = PKT_TRANSFER;
    req.amount = amount;
    strncpy(req.acc_no, sess.acc_no,   20);
    strncpy(req.extra,  target,        20);
    strncpy(req.extra2, sess.db_grade, 11);
    send_pkt(&req);

    Packet res;
    recv_pkt(&res);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   거래 내역 조회 (5건 페이징)
   서버가 행마다 RES_MORE_DATA로 전송,
   클라이언트는 5건 출력 후 사용자에게 묻고,
   남은 패킷은 모두 수신(소켓 버퍼 정합성 유지)
   ──────────────────────────────────────── */
static void client_view_history(void) {
    Packet req, res;
    int page_count = 0, total_count = 0, user_quit = 0;
    char ans;

    memset(&req, 0, sizeof(req));
    req.type = PKT_VIEW_HISTORY;
    strncpy(req.acc_no, sess.acc_no, 20);
    send_pkt(&req);

    printf("\n--- [거래 내역 조회] ---\n");

    while (1) {
        recv_pkt(&res);
        if (res.result == RES_END_DATA) {
            if (total_count == 0)   printf("거래 내역이 존재하지 않습니다.\n");
            else if (!user_quit)    printf("더 이상 내역이 없습니다.\n");
            break;
        }

        if (!user_quit) {
            /* data = "trans_date|trans_type|amount|balance_after" */
            char t_date[31], t_type[21];
            long t_amt = 0, t_bal = 0;
            sscanf(res.data, "%30[^|]|%20[^|]|%ld|%ld",
                   t_date, t_type, &t_amt, &t_bal);

            char amt_str[50], bal_str[50];
            fmt_comma(t_amt, amt_str);
            fmt_comma(t_bal, bal_str);

            printf("[%s] %-10s | 금액: %10s원 | 잔액: %10s원\n",
                   t_date, t_type, amt_str, bal_str);
            page_count++;

            if (page_count == 5) {
                printf("\n다음 페이지를 보시겠습니까? (Y/N): ");
                flush_stdin();
                scanf(" %c", &ans);
                if (ans != 'Y' && ans != 'y') user_quit = 1;
                page_count = 0;
            }
        }
        total_count++;
    }
}

/* ────────────────────────────────────────
   비밀번호 변경
   ──────────────────────────────────────── */
static void client_change_password(void) {
    char cur_pw[64], new_pw[64], chk_pw[64];

    printf("\n현재 PW: ");
    scanf("%63s", cur_pw);
    printf("새 PW(4자리): ");
    scanf("%63s", new_pw);
    printf("새 PW 확인: ");
    scanf("%63s", chk_pw);

    if (strcmp(new_pw, chk_pw) != 0) {
        printf("[실패] 입력값이 일치하지 않음\n");
        return;
    }

    Packet res = do_request(PKT_CHANGE_PASSWORD, sess.acc_no, cur_pw, new_pw, 0);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   계좌 해지
   ──────────────────────────────────────── */
static int client_delete_account(void) {
    char pw[64];

    printf("\n--- [계좌 해지] ---\n");
    printf("해지를 진행하려면 비밀번호를 다시 입력하세요: ");
    scanf("%63s", pw);

    Packet res = do_request(PKT_DELETE_ACCOUNT, sess.acc_no, pw, NULL, 0);
    printf("%s\n", res.data);
    return (res.result == RES_OK) ? 1 : 0;
}

/* ────────────────────────────────────────
   정기 예금 가입
   ──────────────────────────────────────── */
static void client_deposit_savings(void) {
    long amount;

    printf("\n--- [정기 예금 가입] ---\n");
    /* 잔액 먼저 조회해서 표시 */
    Packet bal_res = do_request(PKT_CHECK_BALANCE, sess.acc_no, NULL, NULL, 0);
    if (bal_res.result == RES_OK) {
        long balance = 0, savings = 0, credit = 0;
        sscanf(bal_res.data, "%ld|%ld|%ld", &balance, &savings, &credit);
        char b[50], s[50];
        fmt_comma(balance, b);
        fmt_comma(savings, s);
        printf("현재 일반 잔액: %s원 | 현재 예금 잔액: %s원\n", b, s);
    }

    printf("예금에 넣을 금액: ");
    scanf("%ld", &amount);

    Packet res = do_request(PKT_DEPOSIT_SAVINGS, sess.acc_no, NULL, NULL, amount);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   거래 내역 CSV 다운로드
   서버가 행마다 스트리밍, 클라이언트가 파일 저장
   ──────────────────────────────────────── */
static void client_download_csv(void) {
    Packet req, res;
    char filename[80], trimmed[21];
    FILE *fp;
    int count = 0;

    /* 계좌번호에서 '-' 없이 파일명 생성 */
    strncpy(trimmed, sess.acc_no, 20);
    trimmed[20] = '\0';
    snprintf(filename, sizeof(filename), "history_%s.csv", trimmed);

    fp = fopen(filename, "w");
    if (!fp) { printf("[오류] 파일을 생성할 수 없습니다.\n"); return; }
    fprintf(fp, "거래일시,거래유형,거래금액,거래후잔액\n");

    memset(&req, 0, sizeof(req));
    req.type = PKT_DOWNLOAD_CSV;
    strncpy(req.acc_no, sess.acc_no, 20);
    send_pkt(&req);

    while (1) {
        recv_pkt(&res);
        if (res.result == RES_END_DATA) break;

        char t_date[31], t_type[21];
        long t_amt = 0, t_bal = 0;
        sscanf(res.data, "%30[^|]|%20[^|]|%ld|%ld",
               t_date, t_type, &t_amt, &t_bal);
        fprintf(fp, "%s,%s,%ld,%ld\n", t_date, t_type, t_amt, t_bal);
        count++;
    }

    fclose(fp);
    printf("거래 내역 %d건이 %s 파일로 저장되었습니다.\n", count, filename);
}

/* ────────────────────────────────────────
   관리자: 전체 계좌 현황 조회
   ──────────────────────────────────────── */
static void client_admin_view(void) {
    Packet req, res;
    int count = 0;

    printf("\n==================================================\n");
    printf("           [관리자 모드] 전체 계좌 현황\n");
    printf("==================================================\n");
    printf("%-15s | %-15s | %-8s | %s\n", "계좌번호", "고객명", "등급", "잔액(원)");
    printf("--------------------------------------------------\n");

    memset(&req, 0, sizeof(req));
    req.type = PKT_ADMIN_VIEW;
    send_pkt(&req);

    while (1) {
        recv_pkt(&res);
        if (res.result == RES_END_DATA) break;

        /* data = "acc_no|user_name|grade|balance_str" */
        char acc[21], uname[41], grade[12], bal_str[50];
        sscanf(res.data, "%20[^|]|%40[^|]|%11[^|]|%49[^\n]",
               acc, uname, grade, bal_str);
        printf("%-15s | %-15s | %-8s | %15s\n", acc, uname, grade, bal_str);
        count++;
    }

    printf("--------------------------------------------------\n");
    printf("총 %d개의 계좌가 조회되었습니다.\n", count);
}

/* ────────────────────────────────────────
   관리자: 잠긴 계좌 해제
   ──────────────────────────────────────── */
static void client_unlock_account(void) {
    char acc_no[21];
    printf("\n--- [잠긴 계좌 해제] ---\n");
    printf("해제할 계좌번호 입력: ");
    scanf("%20s", acc_no);

    Packet res = do_request(PKT_UNLOCK_ACCOUNT, acc_no, NULL, NULL, 0);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   관리자: VIP 등급 부여
   ──────────────────────────────────────── */
static void client_grant_vip(void) {
    char acc_no[21];
    printf("\n--- [VIP 등급 부여] ---\n");
    printf("VIP로 승급할 계좌번호 입력: ");
    scanf("%20s", acc_no);

    Packet res = do_request(PKT_GRANT_VIP, acc_no, NULL, NULL, 0);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   관리자: 마이너스 한도 부여
   ──────────────────────────────────────── */
static void client_grant_credit(void) {
    char acc_no[21];
    long new_limit;

    printf("\n--- [마이너스 한도 부여] ---\n");
    printf("한도를 부여할 계좌번호 입력: ");
    scanf("%20s", acc_no);
    printf("새로 부여할 한도 금액 입력 (0 입력 시 한도 제거): ");
    scanf("%ld", &new_limit);

    Packet res = do_request(PKT_GRANT_CREDIT, acc_no, NULL, NULL, new_limit);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   관리자: 일괄 이자 지급
   ──────────────────────────────────────── */
static void client_pay_interest(void) {
    printf("\n--- [일괄 이자 지급] ---\n");
    printf("모든 계좌의 예금 잔액에 연 5%% 이자를 지급합니다.\n");
    printf("--------------------------------------------------\n");

    Packet res = do_request(PKT_PAY_INTEREST, NULL, NULL, NULL, 0);
    printf("%s\n", res.data);
}

/* ────────────────────────────────────────
   관리자: 시스템 통계 대시보드
   ──────────────────────────────────────── */
static void client_system_stats(void) {
    Packet res = do_request(PKT_SYSTEM_STATS, NULL, NULL, NULL, 0);
    if (res.result != RES_OK) { printf("%s\n", res.data); return; }

    long accounts = 0, total_bal = 0, overdraft = 0, vip = 0;
    sscanf(res.data, "%ld|%ld|%ld|%ld", &accounts, &total_bal, &overdraft, &vip);

    char s_acc[30], s_bal[50], s_od[50], s_vip[30];
    fmt_comma(accounts,  s_acc);
    fmt_comma(total_bal, s_bal);
    fmt_comma(overdraft, s_od);
    fmt_comma(vip,       s_vip);

    printf("\n==================================================\n");
    printf("          [관리자] 시스템 통계 대시보드\n");
    printf("==================================================\n");
    printf("  항목                       수치\n");
    printf("--------------------------------------------------\n");
    printf("  총 가입 고객 수        : %15s 명\n", s_acc);
    printf("  은행 총 예치금         : %15s 원\n", s_bal);
    printf("  마이너스 대출 총액     : %15s 원\n", s_od);
    printf("  VIP 등급 고객 수       : %15s 명\n", s_vip);
    printf("==================================================\n");
}

/* ────────────────────────────────────────
   메인 루프
   ──────────────────────────────────────── */
int main(void) {
    int login_status;
    int choice;

    printf("====================================================\n");
    printf("        C-Stock ATM Client v2.0 (TCP/IP)\n");
    printf("====================================================\n");

    connect_server();

    while (1) {
        printf("\n==================================================\n");
        printf("              C-Stock ATM 시스템\n");
        printf("==================================================\n");
        printf(" 1. 로그인 (Login)\n");
        printf(" 2. 신규 계좌 개설 (Sign Up)\n");
        printf(" 3. 시스템 종료 (Exit)\n");
        printf("선택: ");
        scanf("%d", &choice);

        if (choice == 3) {
            printf("[시스템] 안전하게 종료합니다.\n");
            break;
        }
        if (choice == 2) {
            client_create_account();
            continue;
        }
        if (choice != 1) continue;

        login_status = client_login();

        /* ── 관리자 세션 ── */
        if (login_status == 2) {
            while (1) {
                printf("\n--- [슈퍼 관리자 시스템] ---\n");
                printf("1. 전체 계좌 현황 조회\n");
                printf("2. 로그아웃\n");
                printf("3. 잠긴 계좌 해제\n");
                printf("4. VIP 등급 부여\n");
                printf("5. 마이너스 한도 부여\n");
                printf("6. 일괄 이자 지급 (연 5%%)\n");
                printf("7. 시스템 통계 대시보드\n");
                printf("선택: ");
                scanf("%d", &choice);

                if      (choice == 1) client_admin_view();
                else if (choice == 2) break;
                else if (choice == 3) client_unlock_account();
                else if (choice == 4) client_grant_vip();
                else if (choice == 5) client_grant_credit();
                else if (choice == 6) client_pay_interest();
                else if (choice == 7) client_system_stats();
            }
        }
        /* ── 고객 세션 ── */
        else if (login_status == 1) {
            while (1) {
                printf("\n--- [고객 뱅킹 서비스] ---\n");
                printf("1.  잔액 조회\n");
                printf("2.  거래 내역 조회 (HISTORY)\n");
                printf("3.  입금\n");
                printf("4.  출금\n");
                printf("5.  계좌 이체\n");
                printf("6.  비밀번호 변경\n");
                printf("7.  계좌 해지\n");
                printf("8.  정기 예금 가입\n");
                printf("9.  거래 내역 다운로드 (CSV)\n");
                printf("10. 로그아웃\n");
                printf("선택: ");
                scanf("%d", &choice);

                if      (choice == 1)  client_check_balance();
                else if (choice == 2)  client_view_history();
                else if (choice == 3)  client_deposit();
                else if (choice == 4)  client_withdraw();
                else if (choice == 5)  client_transfer();
                else if (choice == 6)  client_change_password();
                else if (choice == 7) {
                    if (client_delete_account() == 1) break;
                }
                else if (choice == 8)  client_deposit_savings();
                else if (choice == 9)  client_download_csv();
                else if (choice == 10) break;
            }
        }
    }

    disconnect_server();
    return 0;
}
