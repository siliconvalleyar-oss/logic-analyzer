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
    GPIOReader();
    ~GPIOReader();

    /**
     * Inicializa el acceso a GPIO.
     * En RPi: abre /dev/gpiomem y mapea los registros.
     * En x86: activa modo simulacion.
     * @return true si se inicio correctamente
     */
    bool init();

    /**
     * Lee el estado de todos los pines GPIO (0-31) en un solo acceso.
     * @return Bitfield: bit N = estado del GPIO N (0=low, 1=high)
     */
    uint32_t read_all();

    /** @return true si esta en modo simulacion (sin hardware GPIO). */
    bool is_simulation() const;

    /** @return Descripcion del modo actual. */
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
