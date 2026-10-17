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

} // namespace net

} // namespace kraft
