//==============================================================================
// gpio.h
// Lectura de pines GPIO via mmap (Raspberry Pi) o simulacion (x86)
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_GPIO_H
#define LOGIC_GPIO_H

#include <cstdint>
#include <string>

/**
 * Lee el estado de todos los pines GPIO del Raspberry Pi.
 *
 * En Raspberry Pi usa mmap a /dev/gpiomem para lectura directa
 * de registros (~5 MSps). En x86_64 genera patrones de simulacion
 * para desarrollo sin hardware.
 */
class GPIOReader {
public:
    /**
     * Construye un lector GPIO.
     *
     * No abre el hardware inmediatamente. Llamar a init()
     * antes de usar read_all().
     *
     * @see init()
     */
    GPIOReader();

    /**
     * Destructor. Cierra el mmap y el fd si estaban abiertos.
     *
     * @note  Seguro llamarlo aunque init() no se haya llamado
     *        o haya fallado (fd_ = -1, gpio_map_ = nullptr).
     * @see close()
     */
    ~GPIOReader();

    /**
     * Inicializa el acceso a GPIO.
     *
     * En RPi: abre /dev/gpiomem y mapea los registros GPIO
     *         en memoria (mmap).
     * En x86: activa modo simulacion con contador interno.
     *
     * @return true si se inicio correctamente,
     *         false si no se pudo abrir /dev/gpiomem
     *
     * @throws std::system_error si mmap() falla
     *
     * @note  Solo llamar una vez. Llamadas multiples no tienen efecto.
     * @see   GPIOReader(), read_all()
     */
    bool init();

    /**
     * Lee el estado de todos los pines GPIO (0-31) en un solo acceso.
     *
     * En RPi: lee el registro GPLEV0 via mmap (~50ns).
     * En x86: genera un contador de simulacion.
     *
     * @return Bitfield: bit N = estado del GPIO N (0=low, 1=high).
     *         Los bits 28-31 son zero (solo GPIO 0-27 validos).
     *
     * @pre   init() debe haberse llamado y retornado true.
     * @see   init()
     */
    uint32_t read_all();

    /**
     * Indica si el lector esta en modo simulacion.
     *
     * @return true si NO hay acceso a hardware GPIO real,
     *         false si usa mmap a /dev/gpiomem
     *
     * @see   mode_string()
     */
    bool is_simulation() const;

    /**
     * Descripcion textual del modo de operacion actual.
     *
     * Ejemplos:
     *   - "GPIO mmap (Raspberry Pi)"
     *   - "Simulation mode (no GPIO hardware)"
     *
     * @return String descriptivo del modo
     *
     * @see   is_simulation()
     */
    std::string mode_string() const;

private:
    bool init_simulation();
    uint32_t simulate_read();
    void close();

    int                fd_             = -1;
    volatile uint32_t* gpio_map_       = nullptr;
    bool               simulation_mode_ = false;
    uint64_t           sim_counter_    = 0;
};

#endif // LOGIC_GPIO_H
