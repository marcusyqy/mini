#pragma once

enum struct Log_Level { info, warn, debug, error };
void console_log_print_line(Log_Level level, const char* message, ...);

#define log_info(...)  console_log_print_line(Log_Level::info, __VA_ARGS__)
#define log_warn(...)  console_log_print_line(Log_Level::warn, __VA_ARGS__)
#define log_debug(...) console_log_print_line(Log_Level::debug, __VA_ARGS__)
#define log_error(...) console_log_print_line(Log_Level::error, __VA_ARGS__)
