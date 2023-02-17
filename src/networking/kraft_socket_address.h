#pragma once

#include "core/kraft_core.h"

#if defined(KRAFT_PLATFORM_MACOS)
#include <sys/socket.h>
#elif defined(KRAFT_PLATFORM_WINDOWS)
#include <winsock2.h>
#endif

namespace kraft
{

struct SocketAddress
{
    SocketAddress();
    SocketAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port);
    SocketAddress(uint32 address, uint16 port);

    inline uint8 GetA() const { return _address >> 24; }
    inline uint8 GetB() const { return _address >> 16 & 0xff; }
    inline uint8 GetC() const { return _address >> 8 & 0xff; }
    inline uint8 GetD() const { return _address & 0xff; }

    inline uint16 GetPortH() const { return ntohs(this->_port); };
    inline uint32 GetAddressH() const { return ntohl(this->_address); }

    inline uint16 GetPortN() const { return this->_port; };
    inline uint32 GetAddressN() const { return this->_address; }
private:
    uint32 _address;
    uint16 _port;
};

}