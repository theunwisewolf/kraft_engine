#include <net/kraft_connection.h>
#include <platform/kraft_threading.h>

#include <stdarg.h>

#include <core/kraft_core_includes.h>

namespace kraft::net {

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

} // namespace kraft::net
