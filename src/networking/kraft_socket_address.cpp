#include "kraft_socket_address.h"

namespace kraft
{

SocketAddress::SocketAddress()
{
    this->_address = 0;
    this->_port = 0;
}

SocketAddress::SocketAddress(uint8 a, uint8 b, uint8 c, uint8 d, uint16 port)
{
    this->_address = htonl(a << 24 | b << 16 | c << 8 | d);
    this->_port = htons(port);
}

SocketAddress::SocketAddress(uint32 address, uint16 port)
{
    this->_address = htonl(address);
    this->_port = htons(port);
}

}