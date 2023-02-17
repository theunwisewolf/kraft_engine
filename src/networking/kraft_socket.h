#pragma once

#include "core/kraft_core.h"
#include "networking/kraft_socket_address.h"

#if defined(KRAFT_PLATFORM_MACOS)
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <unistd.h> // For close
#elif defined(KRAFT_PLATFORM_WINDOWS)
#include <winsock2.h>

typedef int socklen_t;
#endif

namespace kraft
{

struct Socket
{
    static bool Init();
    static bool Shutdown();

    bool Open(uint16 port, bool blocking = false);
    void Close();

    bool Send(SocketAddress address, const void* data, size_t size);
    int Receive(SocketAddress* address, void* data, size_t size);

private:
    int _handle;
};

}