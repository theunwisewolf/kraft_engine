#include <net/kraft_http.h>

#include <core/kraft_core_includes.h>

namespace kraft::net {

HttpHeader* HttpHeadersCopy(ArenaAllocator* arena, HttpHeader* headers, u32 count) {
    if (count == 0) {
        return nullptr;
    }

    HttpHeader* copy = ArenaPushArrayNoZero(arena, HttpHeader, count);
    for (u32 index = 0; index < count; index++) {
        copy[index] = HttpHeader{StringCopy(arena, headers[index].key), StringCopy(arena, headers[index].value)};
    }

    return copy;
}

} // namespace kraft::net
