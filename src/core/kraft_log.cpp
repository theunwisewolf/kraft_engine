#include "kraft_log.h"
#include "platform/kraft_platform.h"

namespace kraft
{

struct Logger g_LoggerInstance = kraft::Logger(); 

bool Logger::Init() 
{
    return true;
}

void Logger::Shutdown() {}

void Logger::Log(LogLevel level, const char* message, ...)
{
    const int BUFFER_SIZE = 32000;
    static const char* levelsPrefix[LogLevel::LOG_LEVEL_NUM_COUNT] = { 
        "[FATAL]:", 
        "[ERROR]:", 
        "[WARN]:", 
        "[INFO]:", 
        "[SUCCESS]:",
    };

    static int colors[LogLevel::LOG_LEVEL_NUM_COUNT] = { 
        Platform::ConsoleColorBGRed,
        Platform::ConsoleColorRed,
        Platform::ConsoleColorRed | Platform::ConsoleColorGreen,
        Platform::ConsoleColorBlue | Platform::ConsoleColorGreen,
        Platform::ConsoleColorGreen,
    };

    int prefixLength = strlen(levelsPrefix[level]);
    char out[BUFFER_SIZE] = {0};

    va_list args;
    va_start(args, message);
    vsnprintf(out + prefixLength + this->Padding, BUFFER_SIZE, message, args);
    va_end(args);

    int i = 0;
    while (i < prefixLength)
    {
        out[i] = levelsPrefix[level][i];
        i++;
    }

    while (i < (prefixLength + this->Padding))
    {
        out[i++] = ' ';
    }

    if (level < LogLevel::LOG_LEVEL_WARN)
    {
        kraft::Platform::ConsoleOutputStringError(out, colors[level]);
    }
    else
    {
        kraft::Platform::ConsoleOutputString(out, colors[level]);
    }
}

}