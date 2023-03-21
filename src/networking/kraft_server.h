#pragma once

#include "networking/kraft_socket.h"
#include "networking/kraft_socket_address.h"
#include "networking/kraft_networking_types.h"

namespace kraft
{

struct Connection
{
    ConnectionState State;
};

struct ServerConfig
{
    SocketAddress       Address;
    ServerUpdateMode    UpdateMode;
    uint64              ReceiveBufferSize = 4096;
};

struct Server
{
    Server() {}
    Server(SocketAddress address, ServerUpdateMode mode);
    Server(ServerConfig config);

    bool Start();
    void Update();
    bool Shutdown();

    PacketReceiveCallback   OnPacketReceive;

private:
    Socket _socket;
    ServerConfig _config;
    char* _receiveBuffer;
};

}