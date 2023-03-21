#pragma once

#include "core/kraft_core.h"

namespace kraft
{

enum ConnectionState
{
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTED,

    CONNECTION_STATE_NUM_COUNT,
};

enum ServerUpdateMode
{
    SERVER_UPDATE_MODE_MANUAL,
    SERVER_UPDATE_MODE_AUTO,
    SERVER_UPDATE_MODE_NUM_COUNT
};

typedef void (*PacketReceiveCallback)(const char* buffer, uint64 size);

}