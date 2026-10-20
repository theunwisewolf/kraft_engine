#pragma once

#include <core/kraft_core.h>

namespace kraft {
struct ArenaAllocator;

namespace net {

struct ConnectionPlatform;

// Represents a blocking TCP connection, optionally with TLS
// Every wait gives up at the deadline or once cancel is set, and
// the first error is the one kept
struct Connection {
    ArenaAllocator* arena;
    f64 deadline; // on Platform::GetAbsoluteTime's clock
    u32 timeout_ms;
    volatile i32* cancel;
    const char* phase; // named in a timeout's error
    String8 error;
    String8 tls_warning;
    u64 bytes_received;
    ConnectionPlatform* platform;
};

// Records an error on the connection, unless it already has one
KRAFT_API void ConnectionFail(Connection* connection, const char* format, ...);

// Returns true once the connection's cancel flag is set
KRAFT_API bool ConnectionCancelled(Connection* connection);

// Resolves `host` and connects to `port`, trying every address the host has
// `out_dns_micros` is set to how long resolving the host took
KRAFT_API bool ConnectionOpen(Connection* connection, String8 host, u16 port, u64* out_dns_micros);

// Fails unless the server agrees to alpn
// A certificate that does not verify sets tls_warning instead of failing,
// so self-signed development servers still work
KRAFT_API bool ConnectionStartTls(Connection* connection, String8 host, String8 alpn, bool skip_verify);

// How long each part of opening a connection took
struct ConnectionTimes {
    u64 dns_micros;
    u64 connect_micros; // the TCP handshake
    u64 tls_micros;
};

// ConnectionOpen and then ConnectionStartTls, made one connection where the system can, which saves a round trip
// `out_times` is set as far as it got, even when it fails
KRAFT_API bool ConnectionOpenTls(Connection* connection, String8 host, u16 port, String8 alpn, bool skip_verify, ConnectionTimes* out_times);

// The same as two steps, timed apart, for a platform or a host that needs them
KRAFT_API bool ConnectionOpenTlsInSteps(Connection* connection, String8 host, u16 port, String8 alpn, bool skip_verify, ConnectionTimes* out_times);

// Sends all of `data`, waiting for the socket as long as the deadline allows
KRAFT_API bool ConnectionSend(Connection* connection, const u8* data, u64 count);

// Returns the byte count, 0 when the peer closed, -1 on error
// The data is valid until the next call
KRAFT_API i64 ConnectionReceive(Connection* connection, const u8** out_data);

// Waits up to `micros` for data to arrive
// Returns 1 if there is data to receive, 0 if none arrived in time, -1 on error
KRAFT_API i32 ConnectionReadableWithin(Connection* connection, u64 micros);

// Returns true while the connection has an open socket
KRAFT_API bool ConnectionIsOpen(Connection* connection);

// Closes the socket and frees the TLS state
KRAFT_API void ConnectionClose(Connection* connection);

struct Listener;

enum ListenerStartStatus {
    LISTENER_START_STATUS_OK,
    LISTENER_START_STATUS_PORT_TAKEN, // another program is listening on that port
    LISTENER_START_STATUS_FAILED,
};

// Starts listening for TCP connections at `port` on every address `host` has, such as both loopback addresses for "localhost"
// A `port` of 0 takes a free one, which ListenerPort returns
// Returns null if no address could be listened on, and `out_status` is optional and says why
KRAFT_API Listener* ListenerStart(String8 host, u16 port, ListenerStartStatus* out_status = nullptr);

// Returns the port the listener is on
KRAFT_API u16 ListenerPort(Listener* listener);

// Waits up to `timeout_ms` for a connection to arrive and opens `out_connection` on it
// `out_connection` is set up as for ConnectionOpen first: its arena, deadline and cancel flag
// Returns 1 when a connection was accepted, 0 when none arrived in time, -1 on error
KRAFT_API i32 ListenerAccept(Listener* listener, u64 timeout_ms, Connection* out_connection);

// Stops listening and frees the listener
KRAFT_API void ListenerStop(Listener* listener);

} // namespace net

} // namespace kraft
