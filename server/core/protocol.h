//==============================================================================
// protocol.h
// Mensajes JSON del protocolo WebSocket del analizador logico
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_PROTOCOL_H
#define LOGIC_PROTOCOL_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "hardware/ring_buffer.h"
#include "analysis/trigger.h"

/**
 * Construye un mensaje JSON de tipo "waveform" para enviar por WebSocket.
 *
 * @param samples      Muestras a incluir
 * @param pins_json    String JSON con la lista de pines, ej: "[2,3,4,5]"
 * @param rate_hz      Frecuencia de muestreo
 * @param trigger_idx  Indice del trigger (-1 si no hubo)
 * @return             String JSON listo para enviar
 */
std::string proto_build_waveform(const std::vector<Sample>& samples,
                                 const std::string& pins_json,
                                 int rate_hz, int trigger_idx,
                                 bool reset = false);

/**
 * Construye un mensaje JSON de tipo "state".
 *
 * @param rate       Frecuencia de muestreo
 * @param pins_json  String JSON con lista de pines
 * @param buf_size   Tamano del buffer
 * @return           String JSON
 */
std::string proto_build_state(int rate, const std::string& pins_json,
                              int buf_size);

/**
 * Construye un mensaje JSON de tipo "config" con la configuracion persistida.
 *
 * @param timebase_us  Timebase en microsegundos
 * @param trigger_pin  Pin de trigger (-1 si desactivado)
 * @param trigger_type Tipo de trigger (rising, falling, etc.)
 * @param pins_json    String JSON con la lista de pines
 * @return             String JSON listo para enviar
 */
std::string proto_build_config(int timebase_us, int trigger_pin,
                               const std::string& trigger_type,
                               const std::string& labels_json = "{}",
                               const std::string& enabled_pins_json = "[]",
                               const std::string& decoder_json = "null",
                               const std::string& pins_json = "[]");

/**
 * Construye un JSON de labels a partir de un map pin->label.
 * @param labels  Mapa de pin a label
 * @return        String JSON, ej: {"2":"CLK","3":"DATA"}
 */
std::string proto_build_labels_json(const std::map<int, std::string>& labels);

/**
 * Extrae el valor de un campo string de un JSON.
 *
 * @param json   String JSON completo
 * @param key    Nombre del campo (ej: "cmd")
 * @return       Valor del campo, o string vacio si no se encuentra
 */
std::string proto_extract_string(const std::string& json,
                                 const std::string& key);

/**
 * Extrae el valor de un campo entero de un JSON.
 *
 * @param json       String JSON completo
 * @param key        Nombre del campo (ej: "pin")
 * @param default_val Valor por defecto si no se encuentra
 * @return           Valor del campo
 */
int proto_extract_int(const std::string& json, const std::string& key,
                      int default_val = 0);

#endif // LOGIC_PROTOCOL_H
