//==============================================================================
// sha1.h
// Implementacion SHA-1 (RFC 3174) para handshake WebSocket
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_SHA1_H
#define LOGIC_SHA1_H

#include <cstdint>
#include <cstring>

/**
 * SHA-1 hash calculator (RFC 3174).
 *
 * Implementacion minimalista de SHA-1 usada exclusivamente
 * para el handshake WebSocket (Sec-WebSocket-Accept).
 *
 * @note No es una implementacion general-purpose. Solo soporta
 *       el flujo: reset() -> update() -> final().
 */
struct SHA1 {
    uint32_t state[5];
    uint64_t count;
    uint8_t  buffer[64];

    SHA1() { reset(); }

    /** Inicializa (o reinicia) el contexto de hash. */
    void reset();

    /**
     * Alimenta datos al hash.
     * @param data  Puntero a los datos
     * @param len   Longitud en bytes
     */
    void update(const uint8_t* data, size_t len);

    /**
     * Finaliza el calculo y escribe el digest de 20 bytes.
     * @param digest[20] Buffer de salida (20 bytes)
     */
    void final(uint8_t digest[20]);

private:
    void transform(const uint8_t* block);
    static uint32_t rotl(uint32_t x, uint32_t n);
};

#endif // LOGIC_SHA1_H
