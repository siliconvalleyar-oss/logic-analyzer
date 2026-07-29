//==============================================================================
// sha1.cpp
// Implementacion SHA-1 (RFC 3174) para handshake WebSocket
// Licencia: MIT
//==============================================================================

#include "sha1.h"

void SHA1::reset() {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
}

void SHA1::update(const uint8_t* data, size_t len) {
    size_t idx = (size_t)(count & 63);
    count += len;
    size_t free = 64 - idx;
    if (len >= free) {
        memcpy(buffer + idx, data, free);
        transform(buffer);
        data += free; len -= free;
        while (len >= 64) { transform(data); data += 64; len -= 64; }
        idx = 0;
    }
    memcpy(buffer + idx, data, len);
}

void SHA1::final(uint8_t digest[20]) {
    uint64_t bits = count * 8;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while ((count & 63) != 56) update(&zero, 1);
    for (int i = 0; i < 8; i++) update(&((uint8_t*)&bits)[7 - i], 1);
    for (int i = 0; i < 5; i++) {
        digest[i*4+0] = (uint8_t)(state[i] >> 24);
        digest[i*4+1] = (uint8_t)(state[i] >> 16);
        digest[i*4+2] = (uint8_t)(state[i] >> 8);
        digest[i*4+3] = (uint8_t)(state[i]);
    }
}

void SHA1::transform(const uint8_t* block) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    for (int i = 16; i < 80; i++)
        w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i<20)       { f=(b&c)|(~b&d); k=0x5A827999; }
        else if (i<40)  { f=b^c^d;        k=0x6ED9EBA1; }
        else if (i<60)  { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
        else            { f=b^c^d;        k=0xCA62C1D6; }
        uint32_t temp = rotl(a,5)+f+e+k+w[i];
        e=d;d=c;c=rotl(b,30);b=a;a=temp;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;
}

uint32_t SHA1::rotl(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}
