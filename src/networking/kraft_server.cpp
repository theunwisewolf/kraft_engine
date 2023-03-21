#include "kraft_server.h"

#include "core/kraft_core.h"
#include "core/kraft_log.h"
#include "core/kraft_memory.h"

#if defined(KRAFT_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#endif

namespace kraft
{

Server::Server(ServerConfig config)
{
    _config = config;
}

Server::Server(SocketAddress address, ServerUpdateMode mode)
{
    _config.Address = address;
    _config.UpdateMode = mode;
}

bool Server::Start()
{
    _socket = Socket();
    if (!_socket.Open(_config.Address.GetPort(), false))
    {
        return false;
    }

    if (_config.UpdateMode == ServerUpdateMode::SERVER_UPDATE_MODE_AUTO)
    {
        // Create a thread that calls ::Update()
    }

    _receiveBuffer = (char*)kraft::Malloc(_config.ReceiveBufferSize, MEMORY_TAG_NETWORK);

    return true;
}

void Server::Update()
{
    SocketAddress sender;
    char senderAddressBuffer[KRAFT_MAX_SOCKET_ADDRESS_STRING_LENGTH];

    int receivedBytes = 0;
    while ((receivedBytes = _socket.Receive(&sender, _receiveBuffer, _config.ReceiveBufferSize)) > 0)
    {
        sender.ToString(senderAddressBuffer);
        KINFO("Received message %s from %s", _receiveBuffer, senderAddressBuffer);
        
        MemZero(_receiveBuffer, _config.ReceiveBufferSize);
    }
}

bool Server::Shutdown()
{
    _socket.Close();
    kraft::Free(_receiveBuffer);

    return true;
}

}