#include "log.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <time.h>

namespace helper {
char buffer[1024];

static const char* to_level_string(Log_Level level) {
  switch (level) {
    case Log_Level::info: return "info";
    case Log_Level::warn: return "warn";
    case Log_Level::debug: return "debug";
    case Log_Level::error: return "error";
  }
  return "UNKNOWN_LEVEL";
}

#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"
#define WHITE   "\x1B[37m"

static const char* to_color(Log_Level level) {
  switch (level) {
    case Log_Level::info: return GREEN;
    case Log_Level::warn: return YELLOW;
    case Log_Level::debug: return CYAN;
    case Log_Level::error: return RED;
  }
  return MAGENTA;
}

#undef NORMAL
#undef RED
#undef GREEN
#undef YELLOW
#undef BLUE
#undef MAGENTA
#undef CYAN
#undef WHITE

} // namespace helper

// @TODO: something we can do to improve this is only log to a proper console similar to how spdlog does it.
// temporary implementation until we can do a better job.
void console_log_print_line(Log_Level level, const char* message, ...) {
  va_list list;
  va_start(list, message);
  int value = vsnprintf(helper::buffer, sizeof(helper::buffer), message, list);
  assert(value >= 0 && value < sizeof(helper::buffer));
#define RESET_COLOR_IN_CONSOLE "\x1B[0m"
  // @TODO: add time here as well?
  // GetSystemTimeAsFileTime
  time_t rawtime;
  struct tm timeinfo;
  time(&rawtime);
  localtime_s(&timeinfo, &rawtime);
  fprintf(
      stdout,
      "[%04d-%02d-%02d %02d:%02d:%02d] [%s%s" RESET_COLOR_IN_CONSOLE "] %s\n",
      timeinfo.tm_year + 1900,  
      timeinfo.tm_mon, 
      timeinfo.tm_mday, 
      timeinfo.tm_hour,  
      timeinfo.tm_min,   
      timeinfo.tm_sec,   
      helper::to_color(level),
      helper::to_level_string(level),
      helper::buffer);
#undef RESET_COLOR_IN_CONSOLE
  va_end(list);
}
