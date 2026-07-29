//==============================================================================
// sha1.h
// SHA-1 hash usando OpenSSL (libcrypto) para handshake WebSocket
// Licencia: MIT
//
// NOTA: Se reemplazo la implementacion custom buggada (SHA1 incorrecto
// para mensajes > 3 bytes) por OpenSSL, que esta disponible tanto en
// Raspberry Pi OS como en Ubuntu/Debian con libssl-dev instalado.
//==============================================================================

#ifndef LOGIC_SHA1_H
#define LOGIC_SHA1_H

#include <cstdint>
#include <cstring>
#include <openssl/sha.h>

/**
 * SHA-1 hash calculator (wrapper sobre OpenSSL).
 *
 * Implementacion de SHA-1 usada exclusivamente
 * para el handshake WebSocket (Sec-WebSocket-Accept).
 *
 * @note Es un wrapper sobre la implementacion SHA-1 de OpenSSL,
 *       que sigue el estandar FIPS 180-4 / RFC 3174.
 *       Llamado Sha1 en vez de SHA1 para evitar conflicto con
 *       la funcion SHA1() de OpenSSL (one-shot convenience).
 */
struct Sha1 {
    SHA_CTX ctx;

    /**
     * Construye un contexto SHA-1 y lo inicializa.
     */
    Sha1() { reset(); }

    /**
     * Inicializa (o reinicia) el contexto de hash.
     */
    void reset() {
        SHA1_Init(&ctx);
    }

    /**
     * Alimenta datos al hash.
     * @param data  Puntero a los datos
     * @param len   Longitud en bytes
     */
    void update(const uint8_t* data, size_t len) {
        SHA1_Update(&ctx, data, len);
    }

    /**
     * Finaliza el calculo y escribe el digest de 20 bytes.
     * @param digest  Buffer de salida (debe tener capacidad para
     *                20 bytes = 160 bits del SHA-1)
     */
    void final(uint8_t digest[20]) {
        SHA1_Final(digest, &ctx);
    }
};

#endif // LOGIC_SHA1_H
