#ifndef LOGGER_H
#define LOGGER_H

typedef enum
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

void init_logger(const char *filename);
void log_message(LogLevel level, const char *format, ...);
void close_logger(void);

#endif