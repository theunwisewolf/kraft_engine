#pragma once

#include <core/kraft_core.h>

namespace kraft {

struct Time
{
    static float64 StartTime;
    static float64 ElapsedTime;
    static float64 DeltaTime;
    static float64 FrameTime;

    static void Start();
    static void Update();
    static void Stop();
    static uint64 Now();
    static String Format(String Format, uint64 Timestamp);
};

}