#include "kraft_socket.h"

#include "core/kraft_log.h"
#include "core/kraft_asserts.h"

#if defined(KRAFT_PLATFORM_WINDOWS)
#pragma comment(lib, "wsock32.lib")
#endif


namespace kraft
{

static bool Initialized = false;

bool Socket::Init()
{
#if defined(KRAFT_PLATFORM_WINDOWS)
    WSADATA data;
    return WSAStartup(MakeWord(2, 2), &data) == NO_ERROR;
#endif

    Initialized = true;
    return true;
}

bool Socket::Shutdown()
{
#if defined(KRAFT_PLATFORM_WINDOWS)
    WSACleanup();
#endif

    Initialized = false;
    return true;
}

bool Socket::Open(uint16 port, bool blocking)
{
    // Create
    int handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == -1)
    {
        KERROR("Failed to create socket | Port = %d", port);
        return false;
    }

    // Bind
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(handle, (sockaddr*)&address, sizeof(address)) < 0)
    {
        KERROR("Failed to bind socket | Port = %d", port);

        this->Close();
        return false;
    }

    // Set to non-blocking
    if (!blocking)
    {
#if defined(KRAFT_PLATFORM_MACOS)
        if (fcntl(handle, F_SETFL, O_NONBLOCK, 1) == -1)
        {
            KERROR("Failed to set socket to non-blocking!");

            this->Close();
            return false;
        }
#elif defined(KRAFT_PLATFORM_WINDOWS)
        DWORD nonBlocking = 1;
        if (ioctlsocket(handle, FIONBIO, &nonBlocking) != 0)
        {
            KERROR("Failed to set socket to non-blocking!");

            this->Close();
            return false;
        }
#endif
    }

    this->_handle = handle;
    return true;
}

bool Socket::Send(SocketAddress address, const void* data, size_t size)
{
    sockaddr_in addr;
    addr.sin_addr.s_addr = address.GetAddressN();
    addr.sin_port = address.GetPortN();
    addr.sin_family = AF_INET;

    int sentBytes = sendto(this->_handle, data, size, 0, (sockaddr*)&addr, sizeof(addr));
    if (sentBytes != size)
    {
        KERROR("Failed to send packet!");
        return false;
    }

    return true;
}

int Socket::Receive(SocketAddress* address, void* data, size_t size)
{
    KASSERT(data);

    sockaddr_storage peerAddress;
    socklen_t peerAddressLength;
    peerAddressLength = sizeof(peerAddress);
    char buffer[512];

    ssize_t receivedBytes = recvfrom(this->_handle, buffer, 512, 0, (sockaddr*)&peerAddress, &peerAddressLength);
    if (receivedBytes == -1)
    {
        // KWARN("recvfrom failed");
        return -1;
    }

    if (receivedBytes > size)
    {
        KWARN("Receive buffer not large enough!");
        receivedBytes = size;
    }

    memset(data, 0, size);
    memcpy(data, (void*)buffer, receivedBytes);

    return receivedBytes;
}

void Socket::Close()
{
#if defined(KRAFT_PLATFORM_MACOS)
    close(this->_handle);
#elif defined(KRAFT_PLATFORM_WINDOWS)
    closesocket(this->_handle);
#endif
}

}