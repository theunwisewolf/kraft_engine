#pragma once

#include "core/kraft_core.h"

namespace kraft {

struct EngineConfig {
    int argc = 0;
    char** argv = 0;
    String8 application_name = {};
    bool console_app = false;
};

struct EngineTickStats {
    u64 frame_id = 0;
    f64 total_ms = 0.0;
    f64 time_update_ms = 0.0;
    f64 input_update_ms = 0.0;
    f64 poll_events_ms = 0.0;
    bool poll_events_result = false;
};

struct EngineStats {
    EngineTickStats current_tick;
    EngineTickStats last_completed_tick;
    u64 tick_counter = 0;
};

struct KRAFT_API Engine {
    static EngineConfig config;

    // The location of the executable without a trailing slash
    static String8 base_path;

    // "true" if the application is running and no exit event was recorded
    static bool running;

    // "true" if the application is the background/minimized
    static bool suspended;

    // Initializes the engine
    static bool Init(const EngineConfig* config);

    // Ticks one frame
    static bool Tick();

    // Draws the frame
    static bool Present();

    // Shutdown
    static void Destroy();

    static const String8Array GetCommandLineArgs();
    static const EngineStats& GetStats();
};

} // namespace kraft
