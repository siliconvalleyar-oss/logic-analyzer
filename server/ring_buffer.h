//==============================================================================
// ring_buffer.h
// Buffer circular lock-free SPSC (Single Producer, Single Consumer)
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_RING_BUFFER_H
#define LOGIC_RING_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <atomic>

/** Una muestra de GPIO con timestamp. */
struct Sample {
    uint64_t timestamp_ns;  ///< Timestamp en nanosegundos
    uint32_t gpio_state;    ///< Bitfield: bit N = estado del GPIO N
};

/**
 * Buffer circular lock-free para un productor y un consumidor (SPSC).
 *
 * El productor (thread de adquisicion) escribe via push().
 * El consumidor (thread de broadcast) lee todo via drain().
 * No necesita mutex porque write_idx_ es local al productor
 * y read_idx_ usa memory_order atomico.
 */
class RingBuffer {
public:
    /** @param capacity  Numero maximo de muestras (default: 4096) */
    explicit RingBuffer(size_t capacity = 4096);

    /**
     * Agrega una muestra al buffer (productor).
     * Si el buffer esta lleno, sobrescribe la muestra mas antigua.
     * @param ts    Timestamp en nanosegundos
     * @param state Bitfield de estados GPIO
     */
    void push(uint64_t ts, uint32_t state);

    /**
     * Extrae todas las muestras disponibles (consumidor).
     * @return Vector con las muestras desde la ultima lectura
     */
    std::vector<Sample> drain();

    /** @return Cantidad de muestras disponibles para leer. */
    size_t size() const;

private:
    size_t              capacity_;
    size_t              write_idx_ = 0;
    std::atomic<size_t> read_idx_{0};
    std::vector<Sample> buffer_;
};

#endif // LOGIC_RING_BUFFER_H
