#pragma once

#include "core/kraft_core.h"

namespace kraft
{

struct Time
{
    static float64 StartTime;
    static float64 ElapsedTime;
    static float64 DeltaTime;

    static void Start();
    static void Update();
    static void Stop();
};

}