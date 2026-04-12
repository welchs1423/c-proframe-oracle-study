#ifndef LOGGER_H
#define LOGGER_H

/* =========================================
   [logger.h] 시스템 감사 로그 모듈
   logs/system.log 에 타임스탬프 + 레벨 포함 기록
   ========================================= */

typedef enum {
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

void write_log(LogLevel level, const char *message);

#endif /* LOGGER_H */
