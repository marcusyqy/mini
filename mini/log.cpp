#include "log.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>

namespace helper {
char buffer[1024];

static const char* to_level_string(Log_Level level) {
  switch (level) {
    case Log_Level::info: return "INFO";
    case Log_Level::warn: return "WARN";
    case Log_Level::debug: return "DEBUG";
    case Log_Level::error: return "ERROR";
  }
  return "UNKNOWN_LEVEL";
}

#define NORMAL   "\x1B[0m"
#define RED      "\x1B[31m"
#define GREEN    "\x1B[32m"
#define YELLOW   "\x1B[33m"
#define BLUE     "\x1B[34m"
#define MAGENTA  "\x1B[35m"
#define CYAN     "\x1B[36m"
#define WHITE    "\x1B[37m"

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

// temporary implementation until we can do a better job.
void console_log_print_line(Log_Level level, const char* message, ...) {
  va_list list;
  va_start(list, message);
  int value = vsnprintf(helper::buffer, sizeof(helper::buffer), message, list);
  assert(value >= 0 && value < sizeof(helper::buffer));
  #define RESET_COLOR_IN_CONSOLE "\x1B[0m"
  // @TODO: add time here as well?
  fprintf(stdout, "%s[%s]: %s\n" RESET_COLOR_IN_CONSOLE, helper::to_color(level), helper::to_level_string(level), helper::buffer);
  #undef RESET_COLOR_IN_CONSOLE 
  va_end(list);
}