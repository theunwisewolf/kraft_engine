#pragma once

#include <core/kraft_core.h>

namespace kraft {

enum LogLevel
{
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_SUCCESS,
    LOG_LEVEL_DEBUG, // For very verbose logging
    LOG_LEVEL_NUM_COUNT,
};

struct KRAFT_API Logger
{
    int  Padding = 1;
    bool EnableColors = true;

    bool Init();
    void Shutdown();
    void Log(LogLevel level, const char* message, ...);
};

KRAFT_API extern struct Logger LoggerInstance;

}

#define KraftLog                    ::kraft::LoggerInstance.Log
#define KRAFT_LOG_FATAL(str, ...)   KraftLog(::kraft::LogLevel::LOG_LEVEL_FATAL, str, ##__VA_ARGS__);
#define KRAFT_LOG_ERROR(str, ...)   KraftLog(::kraft::LogLevel::LOG_LEVEL_ERROR, str, ##__VA_ARGS__);
#define KRAFT_LOG_WARN(str, ...)    KraftLog(::kraft::LogLevel::LOG_LEVEL_WARN, str, ##__VA_ARGS__);
#define KRAFT_LOG_SUCCESS(str, ...) KraftLog(::kraft::LogLevel::LOG_LEVEL_SUCCESS, str, ##__VA_ARGS__);
#define KRAFT_LOG_INFO(str, ...)    KraftLog(::kraft::LogLevel::LOG_LEVEL_INFO, str, ##__VA_ARGS__);
#define KRAFT_LOG_DEBUG(str, ...)   KraftLog(::kraft::LogLevel::LOG_LEVEL_DEBUG, str, ##__VA_ARGS__);

#define KFATAL   KRAFT_LOG_FATAL
#define KERROR   KRAFT_LOG_ERROR
#define KWARN    KRAFT_LOG_WARN
#define KSUCCESS KRAFT_LOG_SUCCESS
#define KINFO    KRAFT_LOG_INFO
#define KDEBUG   KRAFT_LOG_DEBUG