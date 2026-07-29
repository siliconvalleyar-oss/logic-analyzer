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

    /**
     * Construye un contexto SHA-1 y lo inicializa.
     *
     * @note  Es equivalente a llamar SHA1() seguido de reset().
     * @see   reset()
     */
    SHA1() { reset(); }

    /**
     * Inicializa (o reinicia) el contexto de hash.
     *
     * Pone los 5 registros de estado a los valores iniciales
     * del SHA-1 (H0=0x67452301, etc.) y resetea el contador
     * de bytes a cero.
     *
     * @note  Llamar reset() entre distintos mensajes. No es
     *        necesario llamarlo antes del primer uso porque
     *        el constructor ya lo hace.
     *
     * @see   SHA1(), update(), final()
     */
    void reset();

    /**
     * Alimenta datos al hash.
     * @param data  Puntero a los datos
     * @param len   Longitud en bytes
     */
    void update(const uint8_t* data, size_t len);

    /**
     * Finaliza el calculo y escribe el digest de 20 bytes.
     * @param digest  Buffer de salida (debe tener capacidad para
     *                20 bytes = 160 bits del SHA-1)
     */
    void final(uint8_t digest[20]);

private:
    void transform(const uint8_t* block);
    static uint32_t rotl(uint32_t x, uint32_t n);
};

#endif // LOGIC_SHA1_H
