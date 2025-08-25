#include "kraft_log.h"

#include <cstdarg>

#include <core/kraft_string.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>

#include "core/kraft_base_includes.h"

namespace kraft {

struct Logger LoggerInstance = kraft::Logger();

bool Logger::Init()
{
    return true;
}

void Logger::Shutdown()
{}

void Logger::LogWithFileAndLine(LogLevel level, const char* filename, int line, const char* message, ...)
{
    TempArena          scratch = ScratchBegin(0, 0);
    const int          BUFFER_SIZE = 32000;
    static const char* levelsPrefix[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        "FATA", "ERRO", "WARN", "INFO", "SUCC", "DEBU",
    };

    static const char* LevelColor[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        KRAFT_CONSOLE_COLOR_256(134), // Fatal
        KRAFT_CONSOLE_COLOR_256(204), // Error
        KRAFT_CONSOLE_COLOR_256(192), // Warning
        KRAFT_CONSOLE_COLOR_256(86),  // Info
        KRAFT_CONSOLE_COLOR_256(47),  // Success
        KRAFT_CONSOLE_COLOR_256(63),  // Debug
    };

    string8 formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());
    int     prefixLength = 4;
    char    out[BUFFER_SIZE] = { 0 };

    va_list args;
    va_start(args, message);
    StringFormatV(out, BUFFER_SIZE, message, args);
    va_end(args);

    char FilenameBuffer[256] = { 0 };
    filesystem::Basename(filename, FilenameBuffer);

    // Pad filename
    // int FilenameLength = StringLengthClamped(FilenameBuffer, 255);
    // for (int i = FilenameLength; i < 48; i++)
    // {
    //     FilenameBuffer[i] = ' ';
    // }

    // Platform::ConsoleSetColor(Platform::ConsoleColorHiGray);
    // Platform::ConsoleOutputString();

    fprintf(
        stdout,
        KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(240) "<%s:%d> " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR "%s\n",
        formatted_time.count,
        formatted_time.ptr,
        LevelColor[level],
        levelsPrefix[level],
        FilenameBuffer,
        line,
        out
    );
}

void Logger::Log(LogLevel level, const char* message, ...)
{
#if 1
    TempArena          scratch = ScratchBegin(0, 0);
    const int          BUFFER_SIZE = 32000;
    static const char* levelsPrefix[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        "FATA", "ERRO", "WARN", "INFO", "SUCC", "DEBU",
    };

    static const char* LevelColor[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        KRAFT_CONSOLE_COLOR_256(134), // Fatal
        KRAFT_CONSOLE_COLOR_256(204), // Error
        KRAFT_CONSOLE_COLOR_256(192), // Warning
        KRAFT_CONSOLE_COLOR_256(86),  // Info
        KRAFT_CONSOLE_COLOR_256(47),  // Success
        KRAFT_CONSOLE_COLOR_256(63),  // Debug
    };

    string8 formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());
    int     prefixLength = 4;
    char    out[BUFFER_SIZE] = { 0 };

    va_list args;
    va_start(args, message);
    StringFormatV(out, BUFFER_SIZE, message, args);
    va_end(args);

    // Platform::ConsoleSetColor(Platform::ConsoleColorHiGray);
    // Platform::ConsoleOutputString();

    fprintf(
        stdout,
        KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(255) "%s\n",
        formatted_time.count,
        formatted_time.ptr,
        LevelColor[level],
        levelsPrefix[level],
        out
    );
#else
    const int          BUFFER_SIZE = 32000;
    static const char* levelsPrefix[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        "FATA", "ERRO", "WARN", "INFO", "SUCC", "DEBU",
    };

    static int colors[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        Platform::ConsoleColorBGLoRed | Platform::ConsoleColorHiWhite, // Fatal
        Platform::ConsoleColorHiRed,                                   // Error
        Platform::ConsoleColorHiYellow,                                // Warning
        Platform::ConsoleColorHiCyan,                                  // Info
        Platform::ConsoleColorHiGreen,                                 // Success
        Platform::ConsoleColorHiWhite,                                 // Debug
    };

    kraft::String FormattedTime = kraft::Time::Format("%I:%M:%S%p", kraft::Time::Now());
    int           prefixLength = 4;
    int           reservedSize = prefixLength + this->Padding + FormattedTime.Length + this->Padding;
    char          out[BUFFER_SIZE] = { 0 };

    va_list args;
    va_start(args, message);
    StringFormatV(out + reservedSize, BUFFER_SIZE - reservedSize, message, args);
    va_end(args);

    int i = 0;
    while (i < FormattedTime.Length)
    {
        out[i] = FormattedTime[i];
        i++;
    }

    while (i < (FormattedTime.Length + this->Padding))
        out[i++] = ' ';

    StringNCopy(out + i, levelsPrefix[level], prefixLength);
    i += prefixLength;

    while (i < reservedSize)
        out[i++] = ' ';

    out[StringLength(out)] = '\n';
    if (level < LogLevel::LOG_LEVEL_WARN)
    {
        Platform::ConsoleOutputStringError(out, colors[level]);
    }
    else
    {
        Platform::ConsoleOutputString(out, colors[level]);
    }
#endif
}

} // namespace kraft