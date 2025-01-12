#include "kraft_time.h"

#include <time.h>

#include <core/kraft_asserts.h>
#include <core/kraft_string.h>
#include <platform/kraft_platform.h>

namespace kraft {

float64 Time::StartTime = 0;
float64 Time::ElapsedTime = 0;
float64 Time::DeltaTime = 0;
float64 Time::FrameTime = 0;

void Time::Start()
{
    StartTime = Platform::GetAbsoluteTime();
    ElapsedTime = 0;
}

void Time::Update()
{
    ElapsedTime = Platform::GetAbsoluteTime() - StartTime;
}

void Time::Stop()
{
    StartTime = 0;
}

uint64 Time::Now()
{
    return (uint64)time(nullptr);
}

String Time::Format(String Format, uint64 Timestamp)
{
    const uint64 MaxLength = 128;
    String       Out(MaxLength, 0);
    Out.Length = 0;
    time_t     Timer = (time_t)Timestamp;
    struct tm* TimeInfo = localtime(&Timer);

    Out.Length = strftime(*Out, Out.Allocated, *Format, TimeInfo);

    KASSERT(Out.Length <= MaxLength);

    return Out;
}
}