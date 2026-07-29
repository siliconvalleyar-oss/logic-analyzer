//==============================================================================
// websocket.cpp
// Codificacion/Decodificacion de frames WebSocket (RFC 6455)
// Licencia: MIT
//==============================================================================

#include "websocket.h"
#include "sha1.h"
#include <cstring>

static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-5AB9DC11B85B";
// GUID correcto de RFC 6455. No confundir con el typo comun
// "5AB9DC11B85B" (invalido). El correcto es "C5AB0DC85B11".

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string r;
    r.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < len) v |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) v |= data[i + 2];
        r += B64[(v >> 18) & 0x3F];
        r += B64[(v >> 12) & 0x3F];
        r += (i + 1 < len) ? B64[(v >> 6) & 0x3F] : '=';
        r += (i + 2 < len) ? B64[v & 0x3F] : '=';
    }
    return r;
}

std::string ws_encode_text(const std::string& payload) {
    size_t len = payload.size();
    std::string f;
    f.reserve(len + 10);
    f += (char)(0x80 | WS_TEXT);
    if (len < 126) {
        f += (char)len;
    } else if (len <= 0xFFFF) {
        f += (char)126;
        f += (char)((len >> 8) & 0xFF);
        f += (char)(len & 0xFF);
    } else {
        f += (char)127;
        for (int i = 7; i >= 0; i--)
            f += (char)((len >> (i * 8)) & 0xFF);
    }
    f += payload;
    return f;
}

bool ws_decode(const uint8_t* data, size_t len, WS_Frame& f) {
    if (len < 2) return false;
    f.fin     = (data[0] & 0x80) != 0;
    f.opcode  = data[0] & 0x0F;
    f.mask    = (data[1] & 0x80) != 0;
    f.payload_len = data[1] & 0x7F;

    size_t off = 2;
    if (f.payload_len == 126) {
        if (len < 4) return false;
        f.payload_len = ((size_t)data[2] << 8) | data[3];
        off = 4;
    } else if (f.payload_len == 127) {
        if (len < 10) return false;
        f.payload_len = 0;
        for (int i = 0; i < 8; i++)
            f.payload_len = (f.payload_len << 8) | data[2 + i];
        off = 10;
    }

    if (f.mask) {
        if (len < off + 4) return false;
        memcpy(f.masking_key, data + off, 4);
        off += 4;
    }

    if (len < off + f.payload_len) return false;
    f.payload.assign((const char*)(data + off), f.payload_len);

    if (f.mask)
        for (size_t i = 0; i < f.payload_len; i++)
            f.payload[i] ^= f.masking_key[i % 4];

    f.frame_len = off + f.payload_len;
    return true;
}

std::string ws_encode_close() {
    std::string f;
    f += (char)(0x80 | WS_CLOSE);
    f += (char)0;
    return f;
}

std::string ws_encode_pong(const std::string& payload) {
    std::string f;
    f += (char)(0x80 | WS_PONG);
    if (payload.size() < 126) {
        f += (char)payload.size();
    } else {
        f += (char)126;
        f += (char)((payload.size() >> 8) & 0xFF);
        f += (char)(payload.size() & 0xFF);
    }
    f += payload;
    return f;
}

std::string ws_compute_accept_key(const std::string& client_key) {
    std::string concat = client_key + WS_MAGIC;
    SHA1 sha;
    sha.update((const uint8_t*)concat.data(), concat.size());
    uint8_t digest[20];
    sha.final(digest);
    return base64_encode(digest, 20);
}
