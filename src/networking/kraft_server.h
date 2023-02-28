#pragma once

#include "networking/kraft_socket.h"
#include "networking/kraft_socket_address.h"

namespace kraft
{

enum ConnectionState
{
    CONNECTION_STATE_DISCONNECTED,
    CONNECTION_STATE_CONNECTED,
    CONNECTION_STATE_NUM_COUNT,
};

struct Connection
{
    ConnectionState State;
};

enum ServerUpdateMode
{
    SERVER_UPDATE_MODE_MANUAL,
    SERVER_UPDATE_MODE_AUTO,
    SERVER_UPDATE_MODE_NUM_COUNT
};

struct ServerConfig
{
    SocketAddress       Address;
    ServerUpdateMode    UpdateMode;
};

typedef void (*PacketReceiveCallback)(const char* buffer, uint64 size);

struct Server
{
    Server() {}
    Server(SocketAddress address, ServerUpdateMode mode);
    Server(ServerConfig config);

    bool Start();
    void Update();
    bool Shutdown();

    PacketReceiveCallback OnPacketReceive;

private:
    Socket _socket;
    ServerConfig _config;
};

}