#include "logger.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* =========================================
   [logger.c] 시스템 감사 로그 구현
   - 로그 파일: logs/system.log
   - 포맷: [YYYY-MM-DD HH:MM:SS] [LEVEL] message
   ========================================= */

#define LOG_DIR  "logs"
#define LOG_FILE "logs/system.log"

void write_log(LogLevel level, const char *message) {
    FILE *fp;
    time_t now;
    struct tm *t;
    char timestamp[25];
    const char *level_str;

    /* logs 디렉토리가 없으면 자동 생성 */
    mkdir(LOG_DIR, 0755);

    fp = fopen(LOG_FILE, "a");
    if (fp == NULL) return;

    now = time(NULL);
    t   = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    switch (level) {
        case LOG_INFO:  level_str = "INFO "; break;
        case LOG_WARN:  level_str = "WARN "; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        default:        level_str = "INFO "; break;
    }

    fprintf(fp, "[%s] [%s] %s\n", timestamp, level_str, message);
    fclose(fp);
}
