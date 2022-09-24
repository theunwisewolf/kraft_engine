#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace kraft
{

enum LogLevel
{
    LOG_LEVEL_FATAL,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_SUCCESS,
    LOG_LEVEL_NUM_COUNT
};

struct Logger
{
    int  Padding        = 1;
    bool EnableColors   = true;

    bool Init();
    void Shutdown();
    void Log(kraft::LogLevel level, const char* message, ...);
};

extern struct Logger g_LoggerInstance; 

}

#define KraftLog                        kraft::g_LoggerInstance.Log
#define KRAFT_LOG_FATAL(str, ...)       KraftLog(kraft::LogLevel::LOG_LEVEL_FATAL,    str ## "\n", __VA_ARGS__);
#define KRAFT_LOG_ERROR(str, ...)       KraftLog(kraft::LogLevel::LOG_LEVEL_ERROR,    str ## "\n", __VA_ARGS__);
#define KRAFT_LOG_WARN(str, ...)        KraftLog(kraft::LogLevel::LOG_LEVEL_WARN,     str ## "\n", __VA_ARGS__);
#define KRAFT_LOG_INFO(str, ...)        KraftLog(kraft::LogLevel::LOG_LEVEL_INFO,     str ## "\n", __VA_ARGS__);
#define KRAFT_LOG_SUCCESS(str, ...)     KraftLog(kraft::LogLevel::LOG_LEVEL_SUCCESS,  str ## "\n", __VA_ARGS__);

#define KFATAL      KRAFT_LOG_FATAL
#define KERROR      KRAFT_LOG_ERROR
#define KWARN       KRAFT_LOG_WARN
#define KSUCCESS    KRAFT_LOG_SUCCESS
#define KINFO       KRAFT_LOG_INFO