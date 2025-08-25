#include "kraft_log.h"

#include <cstdarg>

#include <core/kraft_string.h>
#include <platform/kraft_filesystem.h>
#include <platform/kraft_platform.h>

namespace kraft {

struct Logger LoggerInstance = kraft::Logger();

bool Logger::Init()
{
    return true;
}

void Logger::Shutdown()
{}

KRAFT_INLINE kraft_internal const char* get_level_prefix(LogLevel level)
{
    static const char* levels_prefix[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        "FATA", "ERRO", "WARN", "INFO", "SUCC", "DEBU",
    };

    return levels_prefix[level];
}

KRAFT_INLINE kraft_internal const char* get_level_color(LogLevel level)
{
    static const char* level_color[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        KRAFT_CONSOLE_COLOR_256(134), // Fatal
        KRAFT_CONSOLE_COLOR_256(204), // Error
        KRAFT_CONSOLE_COLOR_256(192), // Warning
        KRAFT_CONSOLE_COLOR_256(86),  // Info
        KRAFT_CONSOLE_COLOR_256(47),  // Success
        KRAFT_CONSOLE_COLOR_256(63),  // Debug
    };

    return level_color[level];
}

void Logger::LogWithFileAndLine(LogLevel level, const char* filename, int line, const char* message_with_format, ...)
{
    TempArena scratch = ScratchBegin(0, 0);
    String8   formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());

    va_list args;
    va_start(args, message_with_format);
    String8 out = StringFormatV(scratch.arena, message_with_format, args);
    va_end(args);

    char filename_buffer[256] = { 0 };
    filesystem::Basename(filename, filename_buffer);

    // Pad filename
    // int FilenameLength = StringLengthClamped(FilenameBuffer, 255);
    // for (int i = FilenameLength; i < 48; i++)
    // {
    //     FilenameBuffer[i] = ' ';
    // }

    fprintf(
        stdout,
        KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(240) "<%s:%d> " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR "%.*s\n",
        String8VArg(formatted_time),
        get_level_color(level),
        get_level_prefix(level),
        filename_buffer,
        line,
        String8VArg(out)
    );
}

void Logger::Log(LogLevel level, const char* message_with_format, ...)
{
#if 1
    TempArena scratch = ScratchBegin(0, 0);
    String8   formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());

    va_list args;
    va_start(args, message_with_format);
    String8 out = StringFormatV(scratch.arena, message_with_format, args);
    va_end(args);

    fprintf(
        stdout,
        KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(255) "%.*s\n",
        String8VArg(formatted_time),
        get_level_color(level),
        get_level_prefix(level),
        String8VArg(out)
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