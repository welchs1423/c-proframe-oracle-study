#include "../include/cstock.h"

/* =========================================
   [main.c] 시스템 진입점 및 메인 라우팅
   ========================================= */

int main(void) {
    int login_status;
    int choice;
    SessionState sess;

    write_log(LOG_INFO, "========== C-Stock ATM 시스템 시작 ==========");
    printf("1. 시스템 부팅 중...\n");
    connect_db();

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
            write_log(LOG_INFO, "========== C-Stock ATM 시스템 종료 ==========");
            printf("[시스템] 안전하게 종료합니다.\n");
            break;
        }
        if (choice == 2) {
            create_account();
            continue;
        }
        if (choice != 1) continue;

        login_status = login(&sess);

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

                if      (choice == 1) admin_view_accounts();
                else if (choice == 2) break;
                else if (choice == 3) unlock_account();
                else if (choice == 4) grant_vip();
                else if (choice == 5) grant_credit();
                else if (choice == 6) pay_interest();
                else if (choice == 7) system_stats_dashboard();
            }
        }
        /* ── 고객 세션 ── */
        else if (login_status == 1) {
            while (1) {
                printf("\n--- [고객 뱅킹 서비스] ---\n");
                printf("1. 잔액 조회\n");
                printf("2. 거래 내역 조회 (HISTORY)\n");
                printf("3. 입금\n");
                printf("4. 출금\n");
                printf("5. 계좌 이체\n");
                printf("6. 비밀번호 변경\n");
                printf("7. 계좌 해지\n");
                printf("8. 정기 예금 가입\n");
                printf("9. 거래 내역 다운로드 (CSV)\n");
                printf("10. 로그아웃\n");
                printf("선택: ");
                scanf("%d", &choice);

                if      (choice == 1)  check_balance(&sess);
                else if (choice == 2)  view_history(&sess);
                else if (choice == 3)  deposit(&sess);
                else if (choice == 4)  withdraw(&sess);
                else if (choice == 5)  transfer(&sess);
                else if (choice == 6)  change_password(&sess);
                else if (choice == 7) {
                    if (delete_account(&sess) == 1) break;
                }
                else if (choice == 8)  deposit_savings(&sess);
                else if (choice == 9)  download_history_csv(&sess);
                else if (choice == 10) break;
            }
        }
    }

    disconnect_db();
    return 0;
}
