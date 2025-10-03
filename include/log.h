// include/log.h
#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LOG_LEVEL_TRACE = 0,
  LOG_LEVEL_DEBUG = 1,
  LOG_LEVEL_INFO  = 2,
  LOG_LEVEL_WARN  = 3,
  LOG_LEVEL_ERROR = 4,
  LOG_LEVEL_FATAL = 5
} log_level_t;

#define LOG_COLOR_BLACK   "\x1b[30m"
#define LOG_COLOR_RED     "\x1b[31m"
#define LOG_COLOR_GREEN   "\x1b[32m"
#define LOG_COLOR_YELLOW  "\x1b[33m"
#define LOG_COLOR_BLUE    "\x1b[34m"
#define LOG_COLOR_MAGENTA "\x1b[35m"
#define LOG_COLOR_CYAN    "\x1b[36m"
#define LOG_COLOR_WHITE   "\x1b[37m"
#define LOG_COLOR_RESET   "\x1b[0m"

extern log_level_t g_log_level;

void log_init(log_level_t level);
int  log_is_terminal(FILE* stream);
void log_log(log_level_t level, const char* file, int line, const char* func,
             const char* fmt, ...);

#define LOG_TRACE(fmt, ...)                                                    \
  log_log(LOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)                                                    \
  log_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  log_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                                     \
  log_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)                                                    \
  log_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)                                                    \
  log_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOG_H
