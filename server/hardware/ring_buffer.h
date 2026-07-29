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

/**
 * Una muestra de GPIO con timestamp.
 *
 * Representa el estado de todos los pines GPIO en un instante
 * de tiempo. Usada por RingBuffer para la comunicacion entre
 * el thread de adquisicion y el thread de broadcast.
 *
 * @see RingBuffer
 */
struct Sample {
    uint64_t timestamp_ns;  ///< Timestamp absoluto en nanosegundos (clock_gettime)
    uint32_t gpio_state;    ///< Bitfield: bit N = estado del GPIO N (0=low, 1=high)
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
     *
     * Si el buffer esta lleno, sobrescribe la muestra mas antigua
     * (comportamiento circular). No bloquea nunca.
     *
     * @param ts    Timestamp en nanosegundos (de clock_gettime)
     * @param state Bitfield de estados GPIO (bit N = pin N)
     *
     * @note  Solo debe ser llamado desde un solo thread (productor).
     *        No es thread-safe para multiples productores.
     *
     * @see   drain(), size()
     */
    void push(uint64_t ts, uint32_t state);

    /**
     * Extrae todas las muestras disponibles (consumidor).
     *
     * Lee el indice atomico de lectura, copia todas las muestras
     * desde la ultima lectura hasta write_idx_, y actualiza
     * read_idx_. Operacion O(n) donde n = muestras disponibles.
     *
     * @return Vector con todas las muestras desde la ultima llamada
     *         a drain(). Vacio si no hay muestras nuevas.
     *
     * @note  Solo debe ser llamado desde un solo thread (consumidor).
     *        No es thread-safe para multiples consumidores.
     *
     * @see   push(), size()
     */
    std::vector<Sample> drain();

    /**
     * Cantidad de muestras disponibles para leer.
     *
     * @return Numero de muestras entre read_idx_ y write_idx_,
     *         en el rango [0, capacity_]
     *
     * @note  Es una estimacion atomica. El valor exacto puede
     *        cambiar inmediatamente despues por una llamada
     *        concurrente a push() desde otro thread.
     */
    size_t size() const;

private:
    size_t              capacity_;
    size_t              write_idx_ = 0;
    std::atomic<size_t> read_idx_{0};
    std::vector<Sample> buffer_;
};

#endif // LOGIC_RING_BUFFER_H
