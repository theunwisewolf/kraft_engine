namespace kraft {

static u32 InvertShiftXor(u32 hs, u32 s);

u32 MurmurHash(const void* key, int len, u32 seed)
{
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const u32 m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    u32 h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const u8* data = (const u8*)key;
    while (len >= 4)
    {
#ifdef KRAFT_PLATFORM_BIG_ENDIAN
        u32 k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
#else
        u32 k = *(unsigned int*)data;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch (len)
    {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0]; h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

u32 MurmurHashInverse(u32 h, u32 seed)
{
    const u32 m = 0x5bd1e995;
    const u32 minv = 0xe59b19bd; // Multiplicative inverse of m under % 2^32
    const int r = 24;

    h = InvertShiftXor(h, 15);
    h *= minv;
    h = InvertShiftXor(h, 13);

    u32 hforward = seed ^ 4;
    hforward *= m;
    u32 k = hforward ^ h;
    k *= minv;
    k ^= k >> r;
    k *= minv;

#ifdef KRAFT_PLATFORM_BIG_ENDIAN
    char* data = (char*)&k;
    k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
#endif

    return k;
}

u64 MurmurHash64(const void* key, int len, u64 seed)
{
    const u64 m = 0xc6a4a7935bd1e995ULL;
    const int    r = 47;

    u64 h = seed ^ (len * m);

    const u64* data = (const u64*)key;
    const u64* end = data + (len / 8);

    while (data != end)
    {
#ifdef KRAFT_PLATFORM_BIG_ENDIAN
        u64 k = *data++;
        char*  p = (char*)&k;
        char   c;
        c = p[0];
        p[0] = p[7];
        p[7] = c;
        c = p[1];
        p[1] = p[6];
        p[6] = c;
        c = p[2];
        p[2] = p[5];
        p[5] = c;
        c = p[3];
        p[3] = p[4];
        p[4] = c;
#else
        u64 k = *data++;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const u8* data2 = (const u8*)data;
    switch (len & 7)
    {
        case 7: h ^= u64(data2[6]) << 48;
        case 6: h ^= u64(data2[5]) << 40;
        case 5: h ^= u64(data2[4]) << 32;
        case 4: h ^= u64(data2[3]) << 24;
        case 3: h ^= u64(data2[2]) << 16;
        case 2: h ^= u64(data2[1]) << 8;
        case 1: h ^= u64(data2[0]); h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

u64 MurmurHash64Inverse(u64 h, u64 seed)
{
    const u64 m = 0xc6a4a7935bd1e995ULL;
    const u64 minv = 0x5f7a0ea7e59b19bdULL; // Multiplicative inverse of m under % 2^64
    const int    r = 47;

    h ^= h >> r;
    h *= minv;
    h ^= h >> r;
    h *= minv;

    u64 hforward = seed ^ (8 * m);
    u64 k = h ^ hforward;

    k *= minv;
    k ^= k >> r;
    k *= minv;

#ifdef KRAFT_PLATFORM_BIG_ENDIAN
    char* p = (char*)&k;
    char  c;
    c = p[0];
    p[0] = p[7];
    p[7] = c;
    c = p[1];
    p[1] = p[6];
    p[6] = c;
    c = p[2];
    p[2] = p[5];
    p[5] = c;
    c = p[3];
    p[3] = p[4];
    p[4] = c;
#endif

    return k;
}

/// Inverts a (h ^= h >> s) operation with 8 <= s <= 16
static u32 InvertShiftXor(u32 hs, u32 s)
{
    KASSERT(s >= 8 && s <= 16);
    u32 hs0 = hs >> 24;
    u32 hs1 = (hs >> 16) & 0xff;
    u32 hs2 = (hs >> 8) & 0xff;
    u32 hs3 = hs & 0xff;

    u32 h0 = hs0;
    u32 h1 = hs1 ^ (h0 >> (s - 8));
    u32 h2 = (hs2 ^ (h0 << (16 - s)) ^ (h1 >> (s - 8))) & 0xff;
    u32 h3 = (hs3 ^ (h1 << (16 - s)) ^ (h2 >> (s - 8))) & 0xff;

    return (h0 << 24) + (h1 << 16) + (h2 << 8) + h3;
}

static const u32 Sha256RoundConstants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
    0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d,
    0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e,
    0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static u32 Sha256RotateRight(u32 value, u32 count)
{
    return (value >> count) | (value << (32 - count));
}

static void Sha256Block(u32* state, const u8* block)
{
    u32 schedule[64];
    for (u32 index = 0; index < 16; index++)
    {
        schedule[index] = ((u32)block[index * 4] << 24) | ((u32)block[index * 4 + 1] << 16) | ((u32)block[index * 4 + 2] << 8) | (u32)block[index * 4 + 3];
    }

    for (u32 index = 16; index < 64; index++)
    {
        u32 sigma0 = Sha256RotateRight(schedule[index - 15], 7) ^ Sha256RotateRight(schedule[index - 15], 18) ^ (schedule[index - 15] >> 3);
        u32 sigma1 = Sha256RotateRight(schedule[index - 2], 17) ^ Sha256RotateRight(schedule[index - 2], 19) ^ (schedule[index - 2] >> 10);
        schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
    }

    u32 working[8];
    MemCpy(working, state, sizeof(working));
    for (u32 round = 0; round < 64; round++)
    {
        u32 sum1 = Sha256RotateRight(working[4], 6) ^ Sha256RotateRight(working[4], 11) ^ Sha256RotateRight(working[4], 25);
        u32 choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
        u32 first = working[7] + sum1 + choice + Sha256RoundConstants[round] + schedule[round];
        u32 sum0 = Sha256RotateRight(working[0], 2) ^ Sha256RotateRight(working[0], 13) ^ Sha256RotateRight(working[0], 22);
        u32 majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
        u32 second = sum0 + majority;

        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + first;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = first + second;
    }

    for (u32 index = 0; index < 8; index++)
    {
        state[index] += working[index];
    }
}

void Sha256(const void* data, u64 count, u8* out_digest)
{
    u32 state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const u8* bytes = (const u8*)data;
    u64 remaining = count;
    while (remaining >= 64)
    {
        Sha256Block(state, bytes);
        bytes += 64;
        remaining -= 64;
    }

    // The rest, a 1 bit, zeros, then the length in bits, filling one block or two
    u8 last[128] = {};
    if (remaining > 0)
    {
        MemCpy(last, bytes, remaining);
    }

    last[remaining] = 0x80;
    u64 last_count = remaining < 56 ? 64 : 128;
    u64 bit_count = count * 8;
    for (u32 index = 0; index < 8; index++)
    {
        last[last_count - 1 - index] = (u8)(bit_count >> (index * 8));
    }

    Sha256Block(state, last);
    if (last_count == 128)
    {
        Sha256Block(state, last + 64);
    }

    for (u32 index = 0; index < 8; index++)
    {
        out_digest[index * 4] = (u8)(state[index] >> 24);
        out_digest[index * 4 + 1] = (u8)(state[index] >> 16);
        out_digest[index * 4 + 2] = (u8)(state[index] >> 8);
        out_digest[index * 4 + 3] = (u8)state[index];
    }
}

} // namespace kraft