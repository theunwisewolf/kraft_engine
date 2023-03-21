#pragma once

#include "networking/kraft_socket.h"
#include "networking/kraft_socket_address.h"

namespace kraft
{

struct Client
{
    Client() {}
    bool Init(SocketAddress address);
    void Send(void* data, uint64 size);

private:
    Socket          _socket;
    SocketAddress   _address;
};

}