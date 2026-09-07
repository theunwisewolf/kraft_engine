#pragma once

// Pure base layer: types, memory, strings, logging, math. No engine, events or input.
// Consumers that only need the foundation (tools, non-engine apps) include this.

#include "kraft_core.h"
#include "kraft_asserts.h"
#include "kraft_allocators.h"
#include "kraft_strings.h"
#include "kraft_thread_context.h"
#include "kraft_time.h"
#include "kraft_log.h"
#include "kraft_memory.h"
#include "kraft_hash.h"
#include "kraft_lexer.h"
#include "kraft_buffer.h"
#include "kraft_math.h"
