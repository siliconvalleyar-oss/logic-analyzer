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
     *
     * Crea el socket TCP, inicia epoll, lanza los threads de
     * adquisicion y broadcast, y entra en el bucle principal
     * de eventos epoll.
     *
     * @return true si el servidor se inicio correctamente,
     *         false si hubo error en bind/listen/epoll_create
     *
     * @throws std::system_error si epoll_wait() falla
     *                           durante la ejecucion
     *
     * @note  Corre en el thread principal (bloqueante).
     *        Para detenerlo, llamar a stop() desde otro thread
     *        o manejador de senal.
     * @see   stop(), main_loop()
     */
    bool start();

    /**
     * Solicita el cierre ordenado del servidor.
     *
     * Marca running_ = false, lo que causa que el bucle epoll
     * en start() termine. Los threads de adquisicion y
     * broadcast se unen en el destructor.
     *
     * @note  Es segura para llamar desde un manejador de senal
     *        (SIGINT, SIGTERM) o desde otro thread.
     * @see   start(), ~LogicServer()
     */
    void stop();

private:
    // Estado por cliente
    struct ClientState {
        enum State { HTTP_EXPECT, WS_CONNECTED, HTTP_DONE, CLOSED };
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
    void set_writable(int fd);
    void clear_writable(int fd);
    void close_client(int fd);
    void close_client_locked(int fd,
                             std::map<int, ClientState>::iterator& it);

    std::string get_html_page();
    /**
     * Genera un JSON con la lista de pines configurados.
     * Formato: "[2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
     *           17,18,19,20,21,22,23,24,25,26,27]"
     * @return String JSON con el array de pines GPIO
     */
    std::string pins_json() const;

    /**
     * Genera el string de version del servidor.
     * Formato: "Logic Analyzer v1.x.x"
     * @return String con nombre y version del servidor
     */
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

    // Control (run/stop/single)
    std::atomic<bool> paused_{false};
    std::atomic<bool> single_request_{false};
    std::atomic<bool> pending_reset_{false};

    // Trigger
    TriggerConfig trigger_;
};

#endif // LOGIC_SERVER_H
