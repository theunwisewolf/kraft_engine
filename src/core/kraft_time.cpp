#include "kraft_time.h"

#include "platform/kraft_platform.h"

namespace kraft
{

float64 Time::StartTime     = 0;
float64 Time::ElapsedTime   = 0;
float64 Time::DeltaTime   = 0;

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

}