//==============================================================================
// config.h
// Configuracion del servidor desde archivo JSON y argumentos CLI
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_CONFIG_H
#define LOGIC_CONFIG_H

#include <string>
#include <vector>

/** Configuracion completa del servidor. */
struct ServerConfig {
    // Server
    int     http_port   = 8080;
    int     ws_port     = 8080;  // mismo puerto, upgrade a WS

    // Acquisition
    std::vector<int> pins = {2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
                             17,18,19,20,21,22,23,24,25,26,27};
    int     sample_rate_hz = 500000;
    int     buffer_size    = 4096;
    bool    simulate       = false;

    // Logging
    std::string log_file   = "";
    std::string log_level  = "INFO";

    // Trigger
    int     trigger_pin    = -1;
    std::string trigger_type = "rising";
};

/**
 * Parsea argumentos de linea de comandos.
 *
 * Soporta:
 *   -p, --port <port>     Puerto HTTP
 *   -r, --rate <hz>      Frecuencia de muestreo
 *   -c, --config <file>  Archivo de configuracion
 *   --simulate           Modo simulacion
 *   -v, --verbose        Log verboso
 *   -l, --log <file>     Archivo de log
 *   --version            Muestra version
 *   --help               Muestra ayuda
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
 * Imprime ayuda en stdout.
 * @param name  Nombre del ejecutable (argv[0])
 */
void config_print_help(const char* name);

#endif // LOGIC_CONFIG_H
