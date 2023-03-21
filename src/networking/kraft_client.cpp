#include "kraft_client.h"

namespace kraft
{
    
bool Client::Init(SocketAddress address)
{
    _address = address;
    _socket = Socket();
    return _socket.Open(0);
}

void Client::Send(void* data, uint64 size)
{
    _socket.Send(_address, data, size);
}

}