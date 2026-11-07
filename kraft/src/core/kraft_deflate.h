#pragma once

namespace kraft {

struct ArenaAllocator;

enum InflateResult {
    INFLATE_RESULT_OK,
    INFLATE_RESULT_DAMAGED, // not what it says it is, cut off, or failing its checksum
    INFLATE_RESULT_TOO_LARGE, // it would decompress to more than it was allowed
};

// Returns the bytes compressed as one gzip member (RFC 1952)
KRAFT_API String8 GzipEncode(ArenaAllocator* arena, String8 bytes);

// Decompresses gzip (RFC 1952), every member of it, into out_bytes, as long as that comes to max_count bytes or less
KRAFT_API InflateResult GzipDecode(ArenaAllocator* arena, String8 data, u64 max_count, String8* out_bytes);

// Decompresses zlib (RFC 1950), deflate with a two-byte header and an Adler-32 after it, into out_bytes, as long as
// that comes to max_count bytes or less. Data that needs a preset dictionary counts as damaged
KRAFT_API InflateResult ZlibDecode(ArenaAllocator* arena, String8 data, u64 max_count, String8* out_bytes);

} // namespace kraft
