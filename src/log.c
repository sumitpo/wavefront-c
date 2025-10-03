// src/log.c
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

log_level_t g_log_level = LOG_LEVEL_INFO;

void log_init(log_level_t level) {
  g_log_level = level;
}

int log_is_terminal(FILE* stream) {
  if (!stream)
    return 0;
#ifdef _WIN32
  return _isatty(_fileno(stream));
#else
  return isatty(fileno(stream));
#endif
}

static const char* log_get_level_name(log_level_t level) {
  switch (level) {
  case LOG_LEVEL_TRACE:
    return "TRACE";
  case LOG_LEVEL_DEBUG:
    return "DEBUG";
  case LOG_LEVEL_INFO:
    return "INFO";
  case LOG_LEVEL_WARN:
    return "WARN";
  case LOG_LEVEL_ERROR:
    return "ERROR";
  case LOG_LEVEL_FATAL:
    return "FATAL";
  default:
    return "UNKNOWN";
  }
}

static const char* log_get_short_level_name(log_level_t level) {
  switch (level) {
  case LOG_LEVEL_TRACE:
    return "T";
  case LOG_LEVEL_DEBUG:
    return "D";
  case LOG_LEVEL_INFO:
    return "I";
  case LOG_LEVEL_WARN:
    return "W";
  case LOG_LEVEL_ERROR:
    return "E";
  case LOG_LEVEL_FATAL:
    return "F";
  default:
    return "UNKNOWN";
  }
}

static const char* log_get_level_color(log_level_t level) {
  switch (level) {
  case LOG_LEVEL_TRACE:
    return LOG_COLOR_WHITE;
  case LOG_LEVEL_DEBUG:
    return LOG_COLOR_CYAN;
  case LOG_LEVEL_INFO:
    return LOG_COLOR_GREEN;
  case LOG_LEVEL_WARN:
    return LOG_COLOR_YELLOW;
  case LOG_LEVEL_ERROR:
    return LOG_COLOR_RED;
  case LOG_LEVEL_FATAL:
    return LOG_COLOR_MAGENTA;
  default:
    return LOG_COLOR_RESET;
  }
}

void log_log(log_level_t level, const char* file, int line, const char* func,
             const char* fmt, ...) {
  if (level < g_log_level) {
    return;
  }

  time_t     t       = time(NULL);
  struct tm* tm_info = localtime(&t);
  char       time_str[32];
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

  const char* filename   = file;
  const char* last_slash = strrchr(file, '/');
  if (!last_slash)
    last_slash = strrchr(file, '\\');
  if (last_slash)
    filename = last_slash + 1;

  int         use_color  = log_is_terminal(stderr);
  const char* color      = log_get_level_color(level);
  const char* level_name = log_get_short_level_name(level);

  if (use_color) {
    fprintf(stderr, "%s [%s%s%05d%s] %s:%d %s(): ", time_str, color, level_name,
            getpid(), LOG_COLOR_RESET, filename, line, func);
  } else {
    fprintf(stderr, "%s [%s%05d] %s:%d %s(): ", time_str, level_name, getpid(),
            filename, line, func);
  }

  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  fflush(stderr);

  if (level == LOG_LEVEL_FATAL) {
    exit(EXIT_FAILURE);
  }
}
