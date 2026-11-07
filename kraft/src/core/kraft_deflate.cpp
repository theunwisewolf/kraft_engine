namespace kraft {

#define KRAFT_INFLATE_FAST_BITS 9
#define KRAFT_DEFLATE_WINDOW 32768
#define KRAFT_DEFLATE_MAX_MATCH 258
#define KRAFT_DEFLATE_HASH_BITS 15
#define KRAFT_DEFLATE_CHAIN 64
#define KRAFT_DEFLATE_MAX_RATIO 1032

static const u16 DeflateLengthBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8 DeflateLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 DeflateDistanceBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const u8 DeflateDistanceExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static u32 DeflateReverseBits(u32 value, u32 count) {
    u32 reversed = 0;
    for (u32 bit = 0; bit < count; bit++) {
        reversed = (reversed << 1) | ((value >> bit) & 1);
    }
    return reversed;
}

static u32 DeflateReadLittle32(const u8* bytes) {
    return (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16) | ((u32)bytes[3] << 24);
}

static void DeflateWriteLittle32(u8* bytes, u32 value) {
    bytes[0] = (u8)value;
    bytes[1] = (u8)(value >> 8);
    bytes[2] = (u8)(value >> 16);
    bytes[3] = (u8)(value >> 24);
}

static u32 DeflateAdler32(const u8* bytes, u64 count) {
    u32 low = 1;
    u32 high = 0;
    while (count > 0) {
        u64 chunk = math::Min<u64>(count, 5552);
        count -= chunk;
        for (u64 index = 0; index < chunk; index++) {
            low += bytes[index];
            high += low;
        }
        bytes += chunk;
        low %= 65521;
        high %= 65521;
    }
    return (high << 16) | low;
}

// ---------------------------------------------------------------------------------------------
// Inflate
// ---------------------------------------------------------------------------------------------

// A code up to KRAFT_INFLATE_FAST_BITS long is one lookup in `fast`: its length above bit 9, its symbol below.
struct InflateHuffman {
    u16 fast[1 << KRAFT_INFLATE_FAST_BITS];
    u16 first_code[17];
    u16 first_symbol[17];
    u32 max_code[17];
    u8 code_length[288];
    u16 symbol[288];
};

struct Inflater {
    const u8* input;
    u64 input_count;
    u64 input_position;
    u64 bits;
    u32 bit_count;
    ArenaAllocator* arena;
    u8* output;
    u64 output_count;
    u64 output_capacity;
    u64 max_count;
    bool too_large;
};

static bool InflateBuildHuffman(InflateHuffman* huffman, const u8* lengths, u32 count) {
    u32 length_count[17] = {};
    u32 next_code[16] = {};
    MemZero(huffman->fast, sizeof(huffman->fast));
    for (u32 index = 0; index < count; index++) {
        length_count[lengths[index]]++;
    }

    length_count[0] = 0;
    u32 code = 0;
    u32 symbol_index = 0;
    for (u32 length = 1; length < 16; length++) {
        if (length_count[length] > (1u << length)) {
            return false;
        }

        next_code[length] = code;
        huffman->first_code[length] = (u16)code;
        huffman->first_symbol[length] = (u16)symbol_index;
        code += length_count[length];
        if (length_count[length] > 0 && code - 1 >= (1u << length)) {
            return false;
        }

        huffman->max_code[length] = code << (16 - length);
        code <<= 1;
        symbol_index += length_count[length];
    }

    huffman->max_code[16] = 0x10000;
    for (u32 index = 0; index < count; index++) {
        u32 length = lengths[index];
        if (length == 0) {
            continue;
        }

        u32 slot = next_code[length] - huffman->first_code[length] + huffman->first_symbol[length];
        huffman->code_length[slot] = (u8)length;
        huffman->symbol[slot] = (u16)index;
        if (length <= KRAFT_INFLATE_FAST_BITS) {
            for (u32 entry = DeflateReverseBits(next_code[length], length); entry < (1u << KRAFT_INFLATE_FAST_BITS); entry += 1u << length) {
                huffman->fast[entry] = (u16)((length << 9) | index);
            }
        }

        next_code[length]++;
    }

    return true;
}

static void InflateFill(Inflater* inflater) {
    while (inflater->bit_count <= 56 && inflater->input_position < inflater->input_count) {
        inflater->bits |= (u64)inflater->input[inflater->input_position++] << inflater->bit_count;
        inflater->bit_count += 8;
    }
}

static u32 InflateTake(Inflater* inflater, u32 count) {
    u32 value = (u32)(inflater->bits & ((1ull << count) - 1));
    inflater->bits >>= count;
    inflater->bit_count -= count;

    return value;
}

static bool InflateRead(Inflater* inflater, u32 count, u32* out_value) {
    if (inflater->bit_count < count) {
        InflateFill(inflater);
        if (inflater->bit_count < count) {
            return false;
        }
    }
    *out_value = InflateTake(inflater, count);

    return true;
}

// The whole bytes the bit buffer holds go back to the input, for what is read a byte at a time
static void InflateAlign(Inflater* inflater) {
    InflateTake(inflater, inflater->bit_count % 8);
    inflater->input_position -= inflater->bit_count / 8;
    inflater->bits = 0;
    inflater->bit_count = 0;
}

// Returns the next symbol of the code, or -1 when the bits are none of its codes or run out
static i32 InflateDecode(Inflater* inflater, InflateHuffman* huffman) {
    if (inflater->bit_count < 16) {
        InflateFill(inflater);
    }

    u32 fast = huffman->fast[inflater->bits & ((1u << KRAFT_INFLATE_FAST_BITS) - 1)];
    if (fast != 0) {
        u32 length = fast >> 9;
        if (length > inflater->bit_count) {
            return -1;
        }
        InflateTake(inflater, length);
        return (i32)(fast & 511);
    }

    u32 reversed = DeflateReverseBits((u32)(inflater->bits & 0xFFFF), 16);
    u32 length = KRAFT_INFLATE_FAST_BITS + 1;
    while (length < 16 && reversed >= huffman->max_code[length]) {
        length++;
    }

    if (length >= 16 || length > inflater->bit_count) {
        return -1;
    }

    u32 slot = (reversed >> (16 - length)) - huffman->first_code[length] + huffman->first_symbol[length];
    if (slot >= 288 || huffman->code_length[slot] != length) {
        return -1;
    }

    InflateTake(inflater, length);

    return huffman->symbol[slot];
}

static bool InflateReserve(Inflater* inflater, u64 extra) {
    u64 needed = inflater->output_count + extra;
    if (needed > inflater->max_count) {
        inflater->too_large = true;
        return false;
    }

    if (needed <= inflater->output_capacity) {
        return true;
    }

    u64 capacity = inflater->output_capacity ? inflater->output_capacity * 2 : 4096;
    while (capacity < needed) {
        capacity *= 2;
    }

    capacity = math::Min(capacity, inflater->max_count);
    u8* grown = ArenaPushArrayNoZero(inflater->arena, u8, capacity + 1);
    if (inflater->output_count > 0) {
        MemCpy(grown, inflater->output, inflater->output_count);
    }

    inflater->output = grown;
    inflater->output_capacity = capacity;

    return true;
}

static bool InflateStored(Inflater* inflater) {
    InflateAlign(inflater);
    if (inflater->input_count - inflater->input_position < 4) {
        return false;
    }

    const u8* header = inflater->input + inflater->input_position;
    u32 length = (u32)header[0] | ((u32)header[1] << 8);
    u32 complement = (u32)header[2] | ((u32)header[3] << 8);
    inflater->input_position += 4;

    if ((length ^ 0xFFFF) != complement || inflater->input_count - inflater->input_position < length || !InflateReserve(inflater, length)) {
        return false;
    }

    if (length > 0) {
        MemCpy(inflater->output + inflater->output_count, inflater->input + inflater->input_position, length);
    }

    inflater->output_count += length;
    inflater->input_position += length;

    return true;
}

static bool InflateCodes(Inflater* inflater, InflateHuffman* literals, InflateHuffman* distances) {
    for (;;) {
        i32 symbol = InflateDecode(inflater, literals);
        if (symbol < 0) {
            return false;
        }

        if (symbol < 256) {
            if (inflater->output_count >= inflater->output_capacity && !InflateReserve(inflater, 1)) {
                return false;
            }

            inflater->output[inflater->output_count++] = (u8)symbol;
            continue;
        }

        if (symbol == 256) {
            return true;
        }

        u32 length_symbol = (u32)symbol - 257;
        u32 extra = 0;
        if (length_symbol >= 29 || !InflateRead(inflater, DeflateLengthExtra[length_symbol], &extra)) {
            return false;
        }

        u32 length = DeflateLengthBase[length_symbol] + extra;
        i32 distance_symbol = InflateDecode(inflater, distances);
        if (distance_symbol < 0 || distance_symbol >= 30 || !InflateRead(inflater, DeflateDistanceExtra[distance_symbol], &extra)) {
            return false;
        }

        u64 distance = DeflateDistanceBase[distance_symbol] + extra;
        if (distance > inflater->output_count || !InflateReserve(inflater, length)) {
            return false;
        }

        u8* to = inflater->output + inflater->output_count;
        const u8* from = to - distance;
        for (u32 index = 0; index < length; index++) {
            to[index] = from[index];
        }

        inflater->output_count += length;
    }
}

static void InflateFixedTables(InflateHuffman* literals, InflateHuffman* distances) {
    u8 lengths[288];
    MemSet(lengths, 8, 144);
    MemSet(lengths + 144, 9, 112);
    MemSet(lengths + 256, 7, 24);
    MemSet(lengths + 280, 8, 8);
    InflateBuildHuffman(literals, lengths, 288);
    MemSet(lengths, 5, 30);
    InflateBuildHuffman(distances, lengths, 30);
}

static bool InflateDynamicTables(Inflater* inflater, InflateHuffman* literals, InflateHuffman* distances) {
    static const u8 order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    u32 literal_count = 0;
    u32 distance_count = 0;
    u32 length_code_count = 0;
    if (!InflateRead(inflater, 5, &literal_count) || !InflateRead(inflater, 5, &distance_count) || !InflateRead(inflater, 4, &length_code_count)) {
        return false;
    }

    literal_count += 257;
    distance_count += 1;
    length_code_count += 4;
    if (literal_count > 286 || distance_count > 30) {
        return false;
    }

    u8 code_lengths[19] = {};
    for (u32 index = 0; index < length_code_count; index++) {
        u32 value = 0;
        if (!InflateRead(inflater, 3, &value)) {
            return false;
        }

        code_lengths[order[index]] = (u8)value;
    }

    InflateHuffman length_code;
    if (!InflateBuildHuffman(&length_code, code_lengths, 19)) {
        return false;
    }

    u8 lengths[286 + 30];
    u32 total = literal_count + distance_count;
    u32 filled = 0;
    while (filled < total) {
        i32 symbol = InflateDecode(inflater, &length_code);
        if (symbol < 0) {
            return false;
        }

        if (symbol < 16) {
            lengths[filled++] = (u8)symbol;
            continue;
        }

        u32 repeat = 0;
        u8 value = 0;
        if (symbol == 16) {
            if (filled == 0 || !InflateRead(inflater, 2, &repeat)) {
                return false;
            }

            repeat += 3;
            value = lengths[filled - 1];
        } else if (symbol == 17) {
            if (!InflateRead(inflater, 3, &repeat)) {
                return false;
            }

            repeat += 3;
        } else {
            if (!InflateRead(inflater, 7, &repeat)) {
                return false;
            }

            repeat += 11;
        }

        if (filled + repeat > total) {
            return false;
        }

        MemSet(lengths + filled, value, repeat);
        filled += repeat;
    }

    if (lengths[256] == 0) {
        return false;
    }

    return InflateBuildHuffman(literals, lengths, literal_count) && InflateBuildHuffman(distances, lengths + literal_count, distance_count);
}

// Inflates one deflate stream from the input's position, leaving the position just past it
static bool InflateRun(Inflater* inflater) {
    InflateHuffman literals;
    InflateHuffman distances;
    for (u32 final_block = 0; !final_block;) {
        u32 type = 0;
        if (!InflateRead(inflater, 1, &final_block) || !InflateRead(inflater, 2, &type)) {
            return false;
        }

        bool ok = false;
        if (type == 0) {
            ok = InflateStored(inflater);
        } else if (type == 1) {
            InflateFixedTables(&literals, &distances);
            ok = InflateCodes(inflater, &literals, &distances);
        } else if (type == 2) {
            ok = InflateDynamicTables(inflater, &literals, &distances) && InflateCodes(inflater, &literals, &distances);
        }

        if (!ok) {
            return false;
        }
    }

    InflateAlign(inflater);

    return true;
}

static InflateResult InflateFinish(Inflater* inflater, String8* out_bytes) {
    if (!inflater->output) {
        inflater->output = ArenaPushArrayNoZero(inflater->arena, u8, 1);
    }

    inflater->output[inflater->output_count] = 0;
    *out_bytes = String8{.ptr = inflater->output, .count = inflater->output_count};

    return INFLATE_RESULT_OK;
}

static InflateResult InflateFailure(Inflater* inflater) {
    return inflater->too_large ? INFLATE_RESULT_TOO_LARGE : INFLATE_RESULT_DAMAGED;
}

InflateResult GzipDecode(ArenaAllocator* arena, String8 data, u64 max_count, String8* out_bytes) {
    Inflater inflater = {};
    inflater.arena = arena;
    inflater.input = data.ptr;
    inflater.input_count = data.count;
    inflater.max_count = max_count;

    // The last member's size, as far as the input could hold it, saves growing the output
    if (data.count >= 4) {
        u64 hinted = DeflateReadLittle32(data.ptr + data.count - 4);
        InflateReserve(&inflater, math::Min(math::Min(hinted, max_count), data.count * KRAFT_DEFLATE_MAX_RATIO));
    }

    do {
        u64 position = inflater.input_position;
        if (data.count - position < 18) {
            return INFLATE_RESULT_DAMAGED;
        }

        const u8* header = data.ptr + position;
        u8 flags = header[3];
        if (header[0] != 0x1F || header[1] != 0x8B || header[2] != 8 || (flags & 0xE0) != 0) {
            return INFLATE_RESULT_DAMAGED;
        }

        position += 10;
        if (flags & 4) {
            if (data.count - position < 2) {
                return INFLATE_RESULT_DAMAGED;
            }

            position += 2 + ((u32)data.ptr[position] | ((u32)data.ptr[position + 1] << 8));
        }

        for (u8 text_flag = 8; text_flag <= 16; text_flag += 8) {
            if (flags & text_flag) {
                while (position < data.count && data.ptr[position] != 0) {
                    position++;
                }

                position++;
            }
        }

        if (flags & 2) {
            position += 2;
        }

        if (position > data.count) {
            return INFLATE_RESULT_DAMAGED;
        }

        inflater.input_position = position;
        u64 member_start = inflater.output_count;
        if (!InflateRun(&inflater)) {
            return InflateFailure(&inflater);
        }

        if (data.count - inflater.input_position < 8) {
            return INFLATE_RESULT_DAMAGED;
        }

        const u8* trailer = data.ptr + inflater.input_position;
        u64 member_count = inflater.output_count - member_start;
        if (DeflateReadLittle32(trailer) != Crc32(inflater.output + member_start, member_count) || DeflateReadLittle32(trailer + 4) != (u32)member_count) {
            return INFLATE_RESULT_DAMAGED;
        }

        inflater.input_position += 8;
    } while (inflater.input_position < data.count);

    return InflateFinish(&inflater, out_bytes);
}

InflateResult ZlibDecode(ArenaAllocator* arena, String8 data, u64 max_count, String8* out_bytes) {
    if (data.count < 6) {
        return INFLATE_RESULT_DAMAGED;
    }

    u8 method = data.ptr[0];
    u8 flags = data.ptr[1];
    if ((method & 0x0F) != 8 || (method >> 4) > 7 || ((u32)method * 256 + flags) % 31 != 0 || (flags & 0x20) != 0) {
        return INFLATE_RESULT_DAMAGED;
    }

    Inflater inflater = {};
    inflater.arena = arena;
    inflater.input = data.ptr;
    inflater.input_count = data.count;
    inflater.input_position = 2;
    inflater.max_count = max_count;

    if (!InflateRun(&inflater)) {
        return InflateFailure(&inflater);
    }

    if (data.count - inflater.input_position != 4) {
        return INFLATE_RESULT_DAMAGED;
    }

    const u8* trailer = data.ptr + inflater.input_position;
    u32 expected = ((u32)trailer[0] << 24) | ((u32)trailer[1] << 16) | ((u32)trailer[2] << 8) | (u32)trailer[3];
    if (expected != DeflateAdler32(inflater.output, inflater.output_count)) {
        return INFLATE_RESULT_DAMAGED;
    }

    return InflateFinish(&inflater, out_bytes);
}

// ---------------------------------------------------------------------------------------------
// Deflate
// ---------------------------------------------------------------------------------------------

struct DeflateWriter {
    u8* output;
    u64 count;
    u64 bits;
    u32 bit_count;
};

static void DeflatePut(DeflateWriter* writer, u32 value, u32 count) {
    writer->bits |= (u64)value << writer->bit_count;
    writer->bit_count += count;
    while (writer->bit_count >= 8) {
        writer->output[writer->count++] = (u8)writer->bits;
        writer->bits >>= 8;
        writer->bit_count -= 8;
    }
}

// A Huffman code goes out from its most significant bit
static void DeflatePutCode(DeflateWriter* writer, u32 code, u32 length) {
    DeflatePut(writer, DeflateReverseBits(code, length), length);
}

static void DeflateLiteral(DeflateWriter* writer, u32 symbol) {
    if (symbol < 144) {
        DeflatePutCode(writer, 0x30 + symbol, 8);
    } else if (symbol < 256) {
        DeflatePutCode(writer, 0x190 + symbol - 144, 9);
    } else if (symbol < 280) {
        DeflatePutCode(writer, symbol - 256, 7);
    } else {
        DeflatePutCode(writer, 0xC0 + symbol - 280, 8);
    }
}

static void DeflateMatch(DeflateWriter* writer, u32 length, u32 distance) {
    u32 length_symbol = 0;
    while (length_symbol < 28 && DeflateLengthBase[length_symbol + 1] <= length) {
        length_symbol++;
    }

    DeflateLiteral(writer, 257 + length_symbol);
    DeflatePut(writer, length - DeflateLengthBase[length_symbol], DeflateLengthExtra[length_symbol]);
    u32 distance_symbol = 0;
    while (distance_symbol < 29 && DeflateDistanceBase[distance_symbol + 1] <= distance) {
        distance_symbol++;
    }

    DeflatePutCode(writer, distance_symbol, 5);
    DeflatePut(writer, distance - DeflateDistanceBase[distance_symbol], DeflateDistanceExtra[distance_symbol]);
}

static u32 DeflateHash(const u8* bytes) {
    u32 value = (u32)bytes[0] | ((u32)bytes[1] << 8) | ((u32)bytes[2] << 16);

    return (value * 2654435761u) >> (32 - KRAFT_DEFLATE_HASH_BITS);
}

// One block in the fixed Huffman code, matches found through chains of earlier positions with the same three bytes
static void DeflateFixedBlock(ArenaAllocator* scratch, DeflateWriter* writer, String8 bytes) {
    u32* head = ArenaPushArray(scratch, u32, 1u << KRAFT_DEFLATE_HASH_BITS);
    u32* previous = ArenaPushArrayNoZero(scratch, u32, KRAFT_DEFLATE_WINDOW);
    DeflatePut(writer, 1, 1);
    DeflatePut(writer, 1, 2);
    u64 position = 0;
    while (position < bytes.count) {
        u32 best_length = 0;
        u32 best_distance = 0;
        if (bytes.count - position >= 3) {
            u32 hash = DeflateHash(bytes.ptr + position);
            u64 longest = math::Min<u64>(bytes.count - position, KRAFT_DEFLATE_MAX_MATCH);
            u32 candidate = head[hash];
            for (u32 step = 0; candidate != 0 && step < KRAFT_DEFLATE_CHAIN; step++) {
                u64 earlier = candidate - 1;
                if (earlier >= position || position - earlier > KRAFT_DEFLATE_WINDOW) {
                    break;
                }

                if (best_length > 0 && (best_length >= longest || bytes.ptr[earlier + best_length] != bytes.ptr[position + best_length])) {
                    candidate = previous[earlier & (KRAFT_DEFLATE_WINDOW - 1)];
                    continue;
                }

                u32 length = 0;
                while (length < longest && bytes.ptr[earlier + length] == bytes.ptr[position + length]) {
                    length++;
                }

                if (length > best_length) {
                    best_length = length;
                    best_distance = (u32)(position - earlier);
                    if (length == longest) {
                        break;
                    }
                }

                candidate = previous[earlier & (KRAFT_DEFLATE_WINDOW - 1)];
            }

            previous[position & (KRAFT_DEFLATE_WINDOW - 1)] = head[hash];
            head[hash] = (u32)(position + 1);
        }

        if (best_length < 3) {
            DeflateLiteral(writer, bytes.ptr[position]);
            position++;
            continue;
        }

        DeflateMatch(writer, best_length, best_distance);
        for (u64 covered = position + 1; covered < position + best_length && bytes.count - covered >= 3; covered++) {
            u32 hash = DeflateHash(bytes.ptr + covered);
            previous[covered & (KRAFT_DEFLATE_WINDOW - 1)] = head[hash];
            head[hash] = (u32)(covered + 1);
        }

        position += best_length;
    }

    DeflateLiteral(writer, 256);
    if (writer->bit_count > 0) {
        DeflatePut(writer, 0, 8 - writer->bit_count);
    }
}

static void DeflateStoredBlocks(DeflateWriter* writer, String8 bytes) {
    u64 position = 0;
    do {
        u32 chunk = (u32)math::Min<u64>(bytes.count - position, 65535);
        writer->output[writer->count++] = position + chunk == bytes.count ? 1 : 0;
        writer->output[writer->count++] = (u8)chunk;
        writer->output[writer->count++] = (u8)(chunk >> 8);
        writer->output[writer->count++] = (u8)~chunk;
        writer->output[writer->count++] = (u8)(~chunk >> 8);
        if (chunk > 0) {
            MemCpy(writer->output + writer->count, bytes.ptr + position, chunk);
        }
        writer->count += chunk;
        position += chunk;
    } while (position < bytes.count);
}

// Raw deflate: the fixed Huffman block, or stored blocks when those come out smaller
static String8 DeflateCompress(ArenaAllocator* arena, String8 bytes) {
    // A literal takes at most 9 bits, and a match of three or more bytes at most 31
    u64 fixed_bound = bytes.count * 11 / 8 + 16;
    u64 stored_count = bytes.count + 5 * (bytes.count / 65535 + 1);
    DeflateWriter writer = {};
    writer.output = ArenaPushArrayNoZero(arena, u8, math::Max(fixed_bound, stored_count));
    if (bytes.count < 0xFFFFFFFFull) {
        TempArena scratch = ScratchBegin(&arena, 1);
        DeflateFixedBlock(scratch.arena, &writer, bytes);
        ScratchEnd(scratch);
    }

    if (bytes.count >= 0xFFFFFFFFull || writer.count > stored_count) {
        writer = DeflateWriter{.output = writer.output};
        DeflateStoredBlocks(&writer, bytes);
    }

    return String8{.ptr = writer.output, .count = writer.count};
}

String8 GzipEncode(ArenaAllocator* arena, String8 bytes) {
    TempArena scratch = ScratchBegin(&arena, 1);
    String8 deflated = DeflateCompress(scratch.arena, bytes);
    u64 count = 10 + deflated.count + 8;
    u8* output = ArenaPushArrayNoZero(arena, u8, count + 1);
    static const u8 header[10] = {0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0xFF};
    MemCpy(output, header, sizeof(header));
    MemCpy(output + 10, deflated.ptr, deflated.count);
    DeflateWriteLittle32(output + 10 + deflated.count, Crc32(bytes.ptr, bytes.count));
    DeflateWriteLittle32(output + 14 + deflated.count, (u32)bytes.count);
    output[count] = 0;
    ScratchEnd(scratch);

    return String8{.ptr = output, .count = count};
}

} // namespace kraft
