#include <net/kraft_connection.h>
#include <platform/kraft_platform.h>
#include <platform/kraft_threading.h>

#include <stdarg.h>
#include <string.h>

#include <core/kraft_core_includes.h>

namespace kraft::net {

struct ConnectionBuffer {
    ArenaAllocator* arena;
    u8* data;
    u64 count;
    u64 capacity;
};

static void ConnectionBufferReserve(ConnectionBuffer* buffer, u64 extra) {
    if (buffer->count + extra <= buffer->capacity) {
        return;
    }

    u64 new_capacity = buffer->capacity ? buffer->capacity * 2 : 16384;
    while (new_capacity < buffer->count + extra) {
        new_capacity *= 2;
    }

    u8* new_data = ArenaPushArrayNoZero(buffer->arena, u8, new_capacity);
    if (buffer->count) {
        MemCpy(new_data, buffer->data, buffer->count);
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;
}

static void ConnectionBufferAppend(ConnectionBuffer* buffer, const u8* bytes, u64 count) {
    ConnectionBufferReserve(buffer, count);
    if (count) {
        MemCpy(buffer->data + buffer->count, bytes, count);
    }

    buffer->count += count;
}

static void ConnectionBufferConsume(ConnectionBuffer* buffer, u64 count) {
    if (count >= buffer->count) {
        buffer->count = 0;
        return;
    }

    memmove(buffer->data, buffer->data + count, buffer->count - count);
    buffer->count -= count;
}

void ConnectionFail(Connection* connection, const char* format, ...) {
    if (connection->error.count) {
        return;
    }

    va_list args;
    va_start(args, format);
    connection->error = StringFormatV(connection->arena, format, args);
    va_end(args);
}

bool ConnectionCancelled(Connection* connection) {
    return connection->cancel && AtomicLoad32(connection->cancel) != 0;
}

static u64 ConnectionMicrosBetween(f64 start, f64 end) {
    return end > start ? (u64)((end - start) * 1000000.0) : 0;
}

bool ConnectionOpenTlsInSteps(Connection* connection, String8 host, u16 port, String8 alpn, bool skip_verify, ConnectionTimes* out_times) {
    *out_times = {};
    f64 start = Platform::GetAbsoluteTime();
    bool connected = ConnectionOpen(connection, host, port, &out_times->dns_micros);
    f64 connected_time = Platform::GetAbsoluteTime();
    u64 open_micros = ConnectionMicrosBetween(start, connected_time);
    out_times->connect_micros = open_micros > out_times->dns_micros ? open_micros - out_times->dns_micros : 0;
    if (!connected) {
        return false;
    }

    const char* phase = connection->phase;
    connection->phase = "completing the TLS handshake";
    bool secured = ConnectionStartTls(connection, host, alpn, skip_verify);
    connection->phase = phase;
    out_times->tls_micros = ConnectionMicrosBetween(connected_time, Platform::GetAbsoluteTime());

    return secured;
}

#if !defined(KRAFT_PLATFORM_MACOS)
bool ConnectionOpenTls(Connection* connection, String8 host, u16 port, String8 alpn, bool skip_verify, ConnectionTimes* out_times) {
    return ConnectionOpenTlsInSteps(connection, host, port, alpn, skip_verify, out_times);
}
#endif

} // namespace kraft::net
