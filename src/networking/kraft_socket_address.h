#pragma once

#include "core/kraft_core.h"
#include "stdio.h"

#if defined(KRAFT_PLATFORM_MACOS)
#include <sys/socket.h>
#elif defined(KRAFT_PLATFORM_WINDOWS)
#include <winsock2.h>
#undef SetPort
#endif

#define KRAFT_MAX_SOCKET_ADDRESS_STRING_LENGTH 256


namespace kraft
{

struct SocketAddress
{
    SocketAddress();
    SocketAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port);
    SocketAddress(uint32 address, uint16 port);

    inline uint8 GetA() const { return _address >> 24; }
    inline uint8 GetB() const { return _address >> 16; }
    inline uint8 GetC() const { return _address >> 8; }
    inline uint8 GetD() const { return _address; }

    inline uint16 GetPort() const { return this->_port; };
    inline uint32 GetAddress() const { return this->_address; }

    inline void SetAddress(uint32 address) { this->_address = address; }
    inline void SetPort(uint16 port) { this->_port = port; }

    inline void ToString(char* buffer)
    {
        snprintf(buffer, KRAFT_MAX_SOCKET_ADDRESS_STRING_LENGTH, "%d.%d.%d.%d:%d", _data[3], _data[2], _data[1], _data[0], _port);
    }
private:
    union
    {
        uint32 _address;
        uint8 _data[4];
    };

    uint16 _port;
};

}