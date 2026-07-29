//==============================================================================
// trigger.h
// Configuracion y deteccion de disparo por flanco
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_TRIGGER_H
#define LOGIC_TRIGGER_H

#include <string>
#include <cstdint>
#include <vector>
#include "ring_buffer.h"

/** Tipos de disparo soportados. */
enum class TriggerType {
    NONE,    ///< Sin trigger
    RISING,  ///< Flanco ascendente (0→1)
    FALLING, ///< Flanco descendente (1→0)
    BOTH,    ///< Cualquier flanco
    HIGH,    ///< Nivel alto
    LOW      ///< Nivel bajo
};

/** Configuracion completa del trigger. */
struct TriggerConfig {
    int          pin  = -1;    ///< Pin GPIO a monitorear (-1 = desactivado)
    TriggerType  type = TriggerType::NONE;

    /** @return Representacion string del tipo. */
    static std::string type_to_string(TriggerType t);

    /** @return TriggerType desde string ("rising", "falling", etc). */
    static TriggerType from_string(const std::string& s);
};

/**
 * Busca el primer indice donde ocurre la condicion de trigger.
 *
 * @param samples    Vector de muestras
 * @param config     Configuracion del trigger
 * @return           Indice de la muestra donde ocurre el trigger,
 *                   o -1 si no se encuentra.
 */
int trigger_find_index(const std::vector<Sample>& samples,
                       const TriggerConfig& config);

#endif // LOGIC_TRIGGER_H
