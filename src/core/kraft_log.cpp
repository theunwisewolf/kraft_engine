#include "kraft_log.h"
#include "platform/kraft_platform.h"

namespace kraft
{

struct Logger LoggerInstance = kraft::Logger(); 

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
        "[DEBUG]:",
    };

    static int colors[LogLevel::LOG_LEVEL_NUM_COUNT] = { 
        Platform::ConsoleColorBGLoRed | Platform::ConsoleColorHiWhite,  // Fatal
        Platform::ConsoleColorHiRed,                                    // Error
        Platform::ConsoleColorHiYellow,                                 // Warning
        Platform::ConsoleColorHiCyan,                                   // Info
        Platform::ConsoleColorHiGreen,                                  // Success
        Platform::ConsoleColorHiWhite,                                  // Debug
    };

    int prefixLength = (int)strlen(levelsPrefix[level]);
    int reservedSize = prefixLength + this->Padding;
    char out[BUFFER_SIZE] = {0};

    va_list args;
    va_start(args, message);
    vsnprintf(out + reservedSize, BUFFER_SIZE - reservedSize, message, args);
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

    out[strlen(out)] = '\n';
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