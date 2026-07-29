//==============================================================================
// logger.h
// Sistema de logging estructurado con niveles y rotacion
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_LOGGER_H
#define LOGIC_LOGGER_H

#include <string>
#include <cstdio>

/** Niveles de log. */
enum LogLevel {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
};

/**
 * Logger simple con soporte de archivo y niveles.
 *
 * Formato: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [Module] Mensaje
 *
 * Uso tipico:
 * @code
 * Logger::init("/var/log/logic-analyzer.log", LOG_INFO);
 * LOG_INFO("Server", "Listening on port %d", 8080);
 * @endcode
 */
class Logger {
public:
    /**
     * Inicializa el logger.
     * @param filepath  Ruta al archivo de log (vacio = solo stderr)
     * @param min_level Nivel minimo a registrar
     */
    static void init(const std::string& filepath = "", LogLevel min_level = LOG_INFO);

    /** Cierra el archivo de log si estaba abierto. */
    static void shutdown();

    /**
     * Registra un mensaje.
     * @param level   Nivel del mensaje
     * @param module  Modulo que origina el mensaje (ej: "Server", "GPIO")
     * @param fmt     Formato printf-style
     * @param ...     Argumentos variables
     */
    static void log(LogLevel level, const std::string& module,
                    const char* fmt, ...);

    /** Setea el nivel minimo de log. */
    static void set_min_level(LogLevel level);

private:
    static FILE*  file_;
    static LogLevel min_level_;
    static const char* level_str(LogLevel l);
};

// Macros de conveniencia
#define LOG_DEBUG(mod, ...)  Logger::log(LOG_DEBUG, mod, __VA_ARGS__)
#define LOG_INFO(mod, ...)   Logger::log(LOG_INFO, mod, __VA_ARGS__)
#define LOG_WARN(mod, ...)   Logger::log(LOG_WARN, mod, __VA_ARGS__)
#define LOG_ERROR(mod, ...)  Logger::log(LOG_ERROR, mod, __VA_ARGS__)
#define LOG_FATAL(mod, ...)  Logger::log(LOG_FATAL, mod, __VA_ARGS__)

#endif // LOGIC_LOGGER_H
