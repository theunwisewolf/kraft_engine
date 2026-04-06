#pragma once

#define KRAFT_ENABLE_LOGGING_FILENAMES true

namespace kraft {

enum LogLevel {
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_SUCCESS,
    LOG_LEVEL_DEBUG, // For very verbose logging
    LOG_LEVEL_NUM_COUNT,
};

struct KRAFT_API Logger {
    bool Init();
    void Shutdown();
    void LogWithFileAndLine(LogLevel level, const char* filename, int line, const char* message, ...);
    void Log(LogLevel level, const char* message, ...);
};

KRAFT_API extern struct Logger g_logger_instance;

} // namespace kraft

#if KRAFT_ENABLE_LOGGING_FILENAMES
#define KraftLog ::kraft::g_logger_instance.LogWithFileAndLine
#define KRAFT_LOG_EXTRA_ARGS __FILE__, __LINE__,
#else
#define KraftLog ::kraft::g_logger_instance.Log
#define KRAFT_LOG_EXTRA_ARGS
#endif
#define KRAFT_LOG_FATAL(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_FATAL, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);
#define KRAFT_LOG_ERROR(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_ERROR, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);
#define KRAFT_LOG_WARN(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_WARN, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);
#define KRAFT_LOG_SUCCESS(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_SUCCESS, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);
#define KRAFT_LOG_INFO(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_INFO, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);
#define KRAFT_LOG_DEBUG(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_DEBUG, KRAFT_LOG_EXTRA_ARGS str, ##__VA_ARGS__);

#define KFATAL KRAFT_LOG_FATAL
#define KERROR KRAFT_LOG_ERROR
#define KWARN KRAFT_LOG_WARN
#define KSUCCESS KRAFT_LOG_SUCCESS
#define KINFO KRAFT_LOG_INFO
#define KDEBUG KRAFT_LOG_DEBUG