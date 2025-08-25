#include <time.h>

#include <platform/kraft_platform.h>

namespace kraft {

f64 Time::start_time = 0;
f64 Time::elapsed_time = 0;
f64 Time::delta_time = 0;
f64 Time::frame_time = 0;

void Time::Start()
{
    start_time = Platform::GetAbsoluteTime();
    elapsed_time = 0;
}

void Time::Update()
{
    elapsed_time = Platform::GetAbsoluteTime() - start_time;
}

void Time::Stop()
{
    start_time = 0;
}

u64 Time::Now()
{
    return (u64)time(nullptr);
}

string8 Time::Format(ArenaAllocator* arena, char* format, u64 timestamp)
{
    const u64 max_length = 128;
    string8   result = { .ptr = ArenaPushArray(arena, char, max_length), .count = max_length };

    time_t     timer = (time_t)timestamp;
    struct tm* time_info = localtime(&timer);

    result.count = strftime(result.ptr, result.count, format, time_info);
    return result;
}
} // namespace kraft