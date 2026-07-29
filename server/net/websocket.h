//==============================================================================
// websocket.h
// Codificacion/Decodificacion de frames WebSocket (RFC 6455)
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_WEBSOCKET_H
#define LOGIC_WEBSOCKET_H

#include <cstdint>
#include <string>
#include <vector>

/** Opcodes de WebSocket (RFC 6455 Section 11.8). */
enum WS_Opcode : uint8_t {
    WS_CONTINUATION = 0x0,
    WS_TEXT         = 0x1,
    WS_BINARY       = 0x2,
    WS_CLOSE        = 0x8,
    WS_PING         = 0x9,
    WS_PONG         = 0xA
};

/** Frame WebSocket decodificado. */
struct WS_Frame {
    bool        fin          = true;   ///< Bit FIN (ultimo frame)
    uint8_t     opcode       = 0;      ///< Opcode del frame
    bool        mask         = false;  ///< Si tiene mascara
    uint8_t     masking_key[4] = {0};  ///< Clave de mascara (4 bytes)
    size_t      payload_len  = 0;      ///< Longitud del payload
    size_t      frame_len    = 0;      ///< Longitud total del frame
    std::string payload;               ///< Datos del payload (unmasked)
};

/**
 * Codifica un frame WebSocket de tipo TEXT (sin mascara, desde servidor).
 *
 * @param payload  Texto a enviar
 * @return         Frame completo listo para write()
 */
std::string ws_encode_text(const std::string& payload);

/**
 * Decodifica un frame WebSocket recibido (con o sin mascara).
 *
 * @param data   Buffer con datos raw del socket
 * @param len    Longitud del buffer
 * @param[out] f Frame decodificado
 * @return       true si se decodifico un frame completo
 */
bool ws_decode(const uint8_t* data, size_t len, WS_Frame& f);

/**
 * Codifica un frame WebSocket de tipo CLOSE.
 * @return Frame de cierre
 */
std::string ws_encode_close();

/**
 * Codifica un frame WebSocket de tipo PONG.
 * @param payload Payload a incluir (tipicamente el mismo del PING)
 * @return Frame PONG
 */
std::string ws_encode_pong(const std::string& payload);

/**
 * Genera el checksum SHA-1 + Base64 para Sec-WebSocket-Accept.
 *
 * @param client_key  Valor de Sec-WebSocket-Key del cliente
 * @return            Valor de Sec-WebSocket-Accept (28 chars + null)
 */
std::string ws_compute_accept_key(const std::string& client_key);

#endif // LOGIC_WEBSOCKET_H
