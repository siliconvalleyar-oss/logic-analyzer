//==============================================================================
// server.h
// Servidor HTTP + WebSocket con epoll (punto central del sistema)
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_SERVER_H
#define LOGIC_SERVER_H

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include "hardware/gpio.h"
#include "hardware/ring_buffer.h"
#include "analysis/trigger.h"
#include "core/config.h"
#include "net/websocket.h"

/**
 * Servidor principal del analizador logico.
 *
 * Integra:
 * - Lectura GPIO via mmap (o simulacion)
 * - Buffer circular lock-free
 * - Deteccion de trigger
 * - Servidor HTTP para servir la pagina web
 * - Servidor WebSocket para streaming de datos
 *
 * Usa epoll en modo level-triggered para manejo de conexiones.
 */
class LogicServer {
public:
    /**
     * Construye el servidor con la configuracion dada.
     * @param config  Configuracion del servidor
     */
    explicit LogicServer(const ServerConfig& config);

    /** Destructor: detiene el servidor. */
    ~LogicServer();

    /**
     * Inicia el servidor (bloqueante, corre hasta stop()).
     * @return true si se inicio correctamente
     */
    bool start();

    /** Solicita el cierre del servidor. */
    void stop();

private:
    // Estado por cliente
    struct ClientState {
        enum State { HTTP_EXPECT, WS_CONNECTED, CLOSED };
        State       state = HTTP_EXPECT;
        std::string read_buf;
        std::string write_buf;
        std::string ws_key;
        std::vector<uint8_t> frame_buf;
    };

    // Threads
    void polling_loop();
    void broadcast_loop();

    // Epoll
    bool main_loop();
    void accept_new_client();
    void handle_client_read(int fd);
    void handle_client_write(int fd);
    void handle_http(int fd, ClientState& c,
                     std::map<int, ClientState>::iterator& it);
    void handle_ws_frame(int fd, const WS_Frame& frame);
    void serve_html(int fd);
    void close_client(int fd);
    void close_client_locked(int fd,
                             std::map<int, ClientState>::iterator& it);

    std::string get_html_page();
    std::string pins_json() const;
    std::string version_string() const;

    // Config
    ServerConfig config_;

    // Socket / Epoll
    int server_fd_ = -1;
    int epoll_fd_  = -1;
    std::atomic<bool> running_{false};

    // Clientes
    std::map<int, ClientState> clients_;
    std::mutex clients_mutex_;

    // GPIO y buffer
    GPIOReader gpio_;
    RingBuffer buffer_;

    // Threads
    std::thread poll_thread_;
    std::thread broadcast_thread_;

    // Trigger
    TriggerConfig trigger_;
};

#endif // LOGIC_SERVER_H
