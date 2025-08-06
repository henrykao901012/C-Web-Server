#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define localtime_r(timep, result) localtime_s(result, timep)
#endif

#include "logger.h"

static FILE *log_file = NULL;

void init_logger(const char *filename)
{
    log_file = fopen(filename, "a");
    if (!log_file)
    {
        fprintf(stderr, "Warning: Could not open log file %s\n", filename);
    }
}

void log_message(LogLevel level, const char *format, ...)
{
    const char *level_str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    va_list args;

    // 輸出到控制台
    printf("[%s] [%s] ", timestamp, level_str[level]);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");

    // 輸出到日誌檔案
    if (log_file)
    {
        fprintf(log_file, "[%s] [%s] ", timestamp, level_str[level]);
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

void close_logger(void)
{
    if (log_file)
    {
        fclose(log_file);
        log_file = NULL;
    }
}