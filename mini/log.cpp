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

} // namespace helper

// temporary implementation until we can do a better job.
void console_log_print_line(Log_Level level, const char* message, ...) {
  va_list list;
  va_start(list, message);
  int value = vsnprintf(helper::buffer, sizeof(helper::buffer), message, list);
  assert(value >= 0 && value < sizeof(helper::buffer));
  // @TODO: add time here as well?
  fprintf(stdout, "[%s]: %s\n", helper::to_level_string(level), helper::buffer);
  va_end(list);
}