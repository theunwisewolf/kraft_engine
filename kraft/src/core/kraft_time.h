#pragma once

namespace kraft {

struct Time
{
    static f64 start_time;
    static f64 elapsed_time;
    static f64 delta_time;
    static f64 frame_time;

    static void    Start();
    static void    Update();
    static void    Stop();
    static u64     Now();
    static string8 Format(ArenaAllocator* arena, char* format, u64 timestamp);
};

} // namespace kraft