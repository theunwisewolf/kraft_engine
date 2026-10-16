#include <core/kraft_core_includes.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_filesystem.h>

namespace kraft {

struct Logger g_logger_instance = kraft::Logger();

static char g_log_history[KRAFT_LOG_HISTORY_LINES][KRAFT_LOG_HISTORY_LINE_MAX];
static u32 g_log_history_next;
static u32 g_log_history_count;

static void LogHistoryPush(const char* level, const char* file, int line, String8 message) {
    char* slot = g_log_history[g_log_history_next % KRAFT_LOG_HISTORY_LINES];
    snprintf(slot, KRAFT_LOG_HISTORY_LINE_MAX, "%s <%s:%d> %.*s", level, file, line, (int)message.count, (const char*)message.ptr);
    g_log_history_next = (g_log_history_next + 1) % KRAFT_LOG_HISTORY_LINES;
    if (g_log_history_count < KRAFT_LOG_HISTORY_LINES) {
        g_log_history_count++;
    }
}

void LogHistoryPushLine(const char* text) {
    char* slot = g_log_history[g_log_history_next % KRAFT_LOG_HISTORY_LINES];
    snprintf(slot, KRAFT_LOG_HISTORY_LINE_MAX, "%s", text ? text : "");
    g_log_history_next = (g_log_history_next + 1) % KRAFT_LOG_HISTORY_LINES;
    if (g_log_history_count < KRAFT_LOG_HISTORY_LINES) {
        g_log_history_count++;
    }
}

u32 LogHistoryCount() {
    return g_log_history_count;
}

const char* LogHistoryLine(u32 index) {
    if (index >= g_log_history_count) {
        return "";
    }

    u32 oldest = g_log_history_count == KRAFT_LOG_HISTORY_LINES ? g_log_history_next : 0;

    return g_log_history[(oldest + index) % KRAFT_LOG_HISTORY_LINES];
}

bool Logger::Init() {
    return true;
}

void Logger::Shutdown() {}

KRAFT_INLINE kraft_internal const char* get_level_prefix(LogLevel level) {
    static const char* levels_prefix[LOG_LEVEL_NUM_COUNT] = {
        "FATA",
        "ERRO",
        "WARN",
        "INFO",
        "SUCC",
        "DEBU",
    };

    return levels_prefix[level];
}

KRAFT_INLINE kraft_internal const char* get_level_color(LogLevel level) {
    static const char* level_color[LOG_LEVEL_NUM_COUNT] = {
        KRAFT_CONSOLE_COLOR_256(134), // Fatal
        KRAFT_CONSOLE_COLOR_256(204), // Error
        KRAFT_CONSOLE_COLOR_256(192), // Warning
        KRAFT_CONSOLE_COLOR_256(86), // Info
        KRAFT_CONSOLE_COLOR_256(47), // Success
        KRAFT_CONSOLE_COLOR_256(63), // Debug
    };

    return level_color[level];
}

void Logger::LogWithFileAndLine(LogLevel level, const char* filename, int line, const char* message_with_format, ...) {
    TempArena scratch = ScratchBegin(0, 0);
    String8 formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());

    va_list args;
    va_start(args, message_with_format);
    String8 out = StringFormatV(scratch.arena, message_with_format, args);
    va_end(args);

    String8 filename_buffer = fs::Basename(scratch.arena, String8FromCString(filename));
    LogHistoryPush(get_level_prefix(level), (const char*)filename_buffer.ptr, line, out);

    if (Plain) {
        fprintf(stdout, "%.*s %s <%.*s:%d> %.*s\n", String8VArg(formatted_time), get_level_prefix(level), String8VArg(filename_buffer), line, String8VArg(out));
    } else {
        fprintf(
            stdout,
            KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(240) "<%.*s:%d> " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR
                                                                                                                                                     "%.*s\n",
            String8VArg(formatted_time),
            get_level_color(level),
            get_level_prefix(level),
            String8VArg(filename_buffer),
            line,
            String8VArg(out)
        );
    }
    fflush(stdout);

    ScratchEnd(scratch);
}

void Logger::Log(LogLevel level, const char* message_with_format, ...) {
#if 1
    TempArena scratch = ScratchBegin(0, 0);
    String8 formatted_time = Time::Format(scratch.arena, "%I:%M:%S%p", Time::Now());

    va_list args;
    va_start(args, message_with_format);
    String8 out = StringFormatV(scratch.arena, message_with_format, args);
    va_end(args);

    if (Plain) {
        fprintf(stdout, "%.*s %s %.*s\n", String8VArg(formatted_time), get_level_prefix(level), String8VArg(out));
    } else {
        fprintf(
            stdout,
            KRAFT_CONSOLE_COLOR_256(246) "%.*s " KRAFT_CONSOLE_TEXT_FORMAT_BOLD "%s%s " KRAFT_CONSOLE_TEXT_FORMAT_CLEAR KRAFT_CONSOLE_COLOR_256(255) "%.*s\n",
            String8VArg(formatted_time),
            get_level_color(level),
            get_level_prefix(level),
            String8VArg(out)
        );
    }
    fflush(stdout);

    ScratchEnd(scratch);
#else
    const int BUFFER_SIZE = 32000;
    static const char* levelsPrefix[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        "FATA",
        "ERRO",
        "WARN",
        "INFO",
        "SUCC",
        "DEBU",
    };

    static int colors[LogLevel::LOG_LEVEL_NUM_COUNT] = {
        Platform::ConsoleColorBGLoRed | Platform::ConsoleColorHiWhite, // Fatal
        Platform::ConsoleColorHiRed, // Error
        Platform::ConsoleColorHiYellow, // Warning
        Platform::ConsoleColorHiCyan, // Info
        Platform::ConsoleColorHiGreen, // Success
        Platform::ConsoleColorHiWhite, // Debug
    };

    kraft::String FormattedTime = kraft::Time::Format("%I:%M:%S%p", kraft::Time::Now());
    int prefixLength = 4;
    int reservedSize = prefixLength + this->Padding + FormattedTime.Length + this->Padding;
    char out[BUFFER_SIZE] = {0};

    va_list args;
    va_start(args, message);
    StringFormatV(out + reservedSize, BUFFER_SIZE - reservedSize, message, args);
    va_end(args);

    int i = 0;
    while (i < FormattedTime.Length) {
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
    if (level < LogLevel::LOG_LEVEL_WARN) {
        Platform::ConsoleOutputStringError(out, colors[level]);
    } else {
        Platform::ConsoleOutputString(out, colors[level]);
    }
#endif
}

} // namespace kraft