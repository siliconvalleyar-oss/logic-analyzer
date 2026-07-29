//==============================================================================
// config.h
// Configuracion del servidor desde archivo JSON y argumentos CLI
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_CONFIG_H
#define LOGIC_CONFIG_H

#include <string>
#include <vector>
#include <map>

/**
 * Configuracion completa del servidor del analizador logico.
 *
 * Valores por defecto: HTTP en puerto 8080, 26 pines GPIO (2-27),
 * muestreo a 500 kHz, buffer de 4096 muestras.
 *
 * Se carga desde:
 * 1. Archivo JSON (--config config.json)
 * 2. Argumentos CLI (--port --rate --simulate)
 * 3. Valores por defecto (los de abajo)
 *
 * @see config_parse_args(), config_load_file()
 */
struct ServerConfig {
    // Server
    int     http_port   = 8080;  ///< Puerto HTTP/WebSocket
    int     ws_port     = 8080;  ///< mismo puerto, upgrade a WS
    std::string config_path = "config.json"; ///< Ruta al archivo de config guardado

    // Acquisition
    std::vector<int> pins = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                             17,18,19,20,21,22,23,24,25,26,27}; ///< Pines GPIO a muestrear
    int     sample_rate_hz = 500000; ///< Frecuencia de muestreo en Hz
    int     buffer_size    = 4096;   ///< Tamano del ring buffer interno
    bool    simulate       = false;  ///< true = modo simulacion (sin GPIO)

    // Logging
    std::string log_file   = "";    ///< Ruta al archivo de log (vacio = solo stderr)
    std::string log_level  = "INFO"; ///< Nivel minimo: DEBUG, INFO, WARN, ERROR

    // Display (persistido desde frontend)
    int     timebase_us    = 500000; ///< Timebase por division en microsegundos (default 500ms)
    std::map<int, std::string> channel_labels; ///< Labels por GPIO, ej: {2:"CLK", 3:"DATA"}
    std::vector<int> enabled_pins; ///< Pines habilitados (vacio = todos habilitados)

    // Trigger
    int     trigger_pin    = -1;     ///< Pin GPIO para trigger (-1 = desactivado)
    std::string trigger_type = "rising"; ///< rising, falling, both, high, low
};

/**
 * Guarda la configuracion actual en un archivo JSON.
 * @param cfg   Configuracion a guardar
 * @param filepath Ruta al archivo (default: config.json)
 * @return      true si se guardo correctamente
 */
bool config_save_file(const ServerConfig& cfg, const std::string& filepath = "config.json");

/**
 * Parsea argumentos de linea de comandos.
 *
 * Soporta:
 *   @c -p, --port PORT       Puerto HTTP
 *   @c -r, --rate HZ         Frecuencia de muestreo
 *   @c -c, --config FILE     Archivo de configuracion
 *   @c --simulate            Modo simulacion
 *   @c -v, --verbose         Log verboso (DEBUG)
 *   @c -l, --log FILE        Archivo de log
 *   @c --version             Muestra version y sale
 *   @c --help                Muestra esta ayuda y sale
 *
 * @param argc  Argument count de main()
 * @param argv  Argument vector de main()
 * @return      Configuracion parseada
 */
ServerConfig config_parse_args(int argc, char* argv[]);

/**
 * Carga configuracion desde archivo JSON.
 * @param filepath Ruta al archivo config.json
 * @return         Configuracion cargada
 */
ServerConfig config_load_file(const std::string& filepath);

/**
 * Imprime ayuda en stdout con las opciones disponibles.
 *
 * Muestra:
 *   @c -p, --port PORT       Puerto HTTP (default: 8080)
 *   @c -r, --rate HZ         Frecuencia de muestreo (default: 500000)
 *   @c -c, --config FILE     Archivo de configuracion
 *   @c --simulate            Modo simulacion (sin GPIO)
 *   @c -v, --verbose         Log verboso (DEBUG)
 *   @c -l, --log FILE        Archivo de log
 *   @c --version             Muestra version y sale
 *   @c --help                Muestra esta ayuda y sale
 *
 * @param name  Nombre del ejecutable (argv[0]) para el usage
 *
 * @note  Esta funcion no retorna (llama a exit(0) al finalizar)
 * @see   config_parse_args()
 */
void config_print_help(const char* name);

#endif // LOGIC_CONFIG_H
