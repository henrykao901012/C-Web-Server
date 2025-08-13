// logger.h - 日誌系統頭文件
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// 現有的函數
void init_logger(const char *filename);
void log_message(LogLevel level, const char *format, ...);
void close_logger(void);

// 便捷宏定義，使用 log_message
#define log_debug(...) log_message(LOG_DEBUG, __VA_ARGS__)
#define log_info(...) log_message(LOG_INFO, __VA_ARGS__)
#define log_warning(...) log_message(LOG_WARNING, __VA_ARGS__)
#define log_error(...) log_message(LOG_ERROR, __VA_ARGS__)

#endif // LOGGER_H