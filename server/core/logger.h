//==============================================================================
// logger.h
// Sistema de logging estructurado con niveles y rotacion
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_LOGGER_H
#define LOGIC_LOGGER_H

#include <string>
#include <cstdio>

/**
 * Niveles de log del sistema.
 *
 * Orden ascendente de severidad. Solo se registran mensajes
 * con nivel >= min_level configurado.
 *
 * @see Logger::init(), Logger::set_min_level()
 */
enum LogLevel {
    LOG_DEBUG = 0, ///< Informacion detallada para depuracion
    LOG_INFO  = 1, ///< Eventos normales de operacion
    LOG_WARN  = 2, ///< Condiciones anormales pero recuperables
    LOG_ERROR = 3, ///< Errores que afectan una operacion pero no el servidor
    LOG_FATAL = 4  ///< Errores criticos que requieren cierre del servidor
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

    /**
     * Setea el nivel minimo de log.
     *
     * Los mensajes con nivel menor a level seran ignorados.
     * Ejemplo: LOG_INFO permite INFO, WARN, ERROR, FATAL;
     *          LOG_DEBUG permite todos.
     *
     * @param level  Nuevo nivel minimo (LOG_DEBUG = mas verbose,
     *               LOG_FATAL = solo errores fatales)
     *
     * @note  Es segura para llamar en cualquier momento,
     *        incluso con otros threads haciendo log.
     * @see   Logger::init()
     */
    static void set_min_level(LogLevel level);

private:
    static FILE*  file_;
    static LogLevel min_level_;
    static const char* level_str(LogLevel l);
};

// Macros de conveniencia
/**
 * @def LOG_DEBUG(mod, ...)
 * Registra un mensaje de depuracion.
 * @param mod    Nombre del modulo (ej: "Server", "GPIO")
 * @param ...    Formato printf-style y argumentos
 */
#define LOG_DEBUG(mod, ...)  Logger::log(LOG_DEBUG, mod, __VA_ARGS__)
/**
 * @def LOG_INFO(mod, ...)
 * Registra un mensaje informativo.
 * @param mod    Nombre del modulo
 * @param ...    Formato printf-style y argumentos
 */
#define LOG_INFO(mod, ...)   Logger::log(LOG_INFO, mod, __VA_ARGS__)
/**
 * @def LOG_WARN(mod, ...)
 * Registra un mensaje de advertencia.
 * @param mod    Nombre del modulo
 * @param ...    Formato printf-style y argumentos
 */
#define LOG_WARN(mod, ...)   Logger::log(LOG_WARN, mod, __VA_ARGS__)
/**
 * @def LOG_ERROR(mod, ...)
 * Registra un mensaje de error.
 * @param mod    Nombre del modulo
 * @param ...    Formato printf-style y argumentos
 */
#define LOG_ERROR(mod, ...)  Logger::log(LOG_ERROR, mod, __VA_ARGS__)
/**
 * @def LOG_FATAL(mod, ...)
 * Registra un mensaje fatal (usualmente seguido de exit).
 * @param mod    Nombre del modulo
 * @param ...    Formato printf-style y argumentos
 */
#define LOG_FATAL(mod, ...)  Logger::log(LOG_FATAL, mod, __VA_ARGS__)

#endif // LOGIC_LOGGER_H
