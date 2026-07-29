//==============================================================================
// main.cpp
// Punto de entrada del servidor del analizador logico
// Licencia: MIT
//==============================================================================

#include <csignal>
#include <iostream>
#include <atomic>

#include "core/config.h"
#include "core/server.h"
#include "core/version.h"
#include "core/logger.h"

static std::atomic<bool> g_shutdown(false);

static void signal_handler(int) {
    g_shutdown = true;
}

/**
 * Punto de entrada principal.
 * Parsea argumentos, inicia el servidor y espera la senal de cierre.
 */
int main(int argc, char* argv[]) {
    // Parsear configuracion
    ServerConfig config = config_parse_args(argc, argv);

    // Inicializar logging
    Logger::init(config.log_file);
    // Aplicar nivel de log desde config
    {
        LogLevel lvl = LOG_INFO;
        if      (config.log_level == "DEBUG") lvl = LOG_DEBUG;
        else if (config.log_level == "WARN")  lvl = LOG_WARN;
        else if (config.log_level == "ERROR") lvl = LOG_ERROR;
        else if (config.log_level == "FATAL") lvl = LOG_FATAL;
        Logger::set_min_level(lvl);
    }
    LOG_INFO("Main", "Logic Analyzer Server v%s starting...",
             LOGIC_VERSION_STRING);

    // Configurar signal handlers
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    // Activar simulacion realista si se solicito
    LogicServer server(config);
    if (config.realistic_sim) {
        server.enable_realistic_sim();
        LOG_INFO("Main", "Realistic simulation enabled (jitter + glitches)");
    }
    if (!server.start()) {
        LOG_ERROR("Main", "Failed to start server on port %d",
                  config.http_port);
        Logger::shutdown();
        return 1;
    }

    // Esperar shutdown
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("Main", "Shutting down...");
    server.stop();
    Logger::shutdown();
    std::cout << "[Server] Done." << std::endl;
    return 0;
}
