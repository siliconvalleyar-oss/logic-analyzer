//==============================================================================
// server.cpp
// Implementacion del servidor HTTP + WebSocket + epoll
// Licencia: MIT
//==============================================================================

#include "server.h"
#include "net/websocket.h"
#include "core/protocol.h"
#include "core/version.h"
#include "core/logger.h"
#include "core/config.h"

#include <cstring>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <sstream>
#include <chrono>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static constexpr int MAX_EVENTS       = 64;
static constexpr int SEND_INTERVAL_MS = 33;
static constexpr int MAX_GPIO_PINS    = 26;

static const int ALL_GPIO_PINS[MAX_GPIO_PINS] = {
    2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
    17,18,19,20,21,22,23,24,25,26,27
};

//------------------------------------------------------------------------------
// Constructor / Destructor
//------------------------------------------------------------------------------

LogicServer::LogicServer(const ServerConfig& config)
    : config_(config)
    , buffer_(config_.buffer_size) {
}

LogicServer::~LogicServer() { stop(); }

//------------------------------------------------------------------------------
// Start
//------------------------------------------------------------------------------

bool LogicServer::start() {
    if (!gpio_.init()) {
        LOG_ERROR("Server", "GPIO init failed");
        return false;
    }

    server_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd_ < 0) {
        LOG_ERROR("Server", "socket() failed: %s", strerror(errno));
        return false;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // NOTA: SO_REUSEPORT deliberadamente NO usado.
    // Permite que procesos zombies compitan por el mismo puerto,
    // enviando handshakes WebSocket incorrectos al navegador.
    // Sin REUSEPORT, si hay un zombie, el nuevo server falla al
    // hacer bind() con un error claro en los logs.

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(config_.http_port);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("Server", "bind() failed on port %d: %s",
                  config_.http_port, strerror(errno));
        return false;
    }

    if (listen(server_fd_, 16) < 0) {
        LOG_ERROR("Server", "listen() failed: %s", strerror(errno));
        return false;
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        LOG_ERROR("Server", "epoll_create1() failed: %s", strerror(errno));
        return false;
    }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

    running_ = true;
    poll_thread_       = std::thread(&LogicServer::polling_loop, this);
    broadcast_thread_  = std::thread(&LogicServer::broadcast_loop, this);

    LOG_INFO("Server", "Listening on port %d", config_.http_port);
    LOG_INFO("Server", "Mode: %s", gpio_.mode_string().c_str());
    LOG_INFO("Server", "Rate: %d Hz", config_.sample_rate_hz);

    return main_loop();
}

//------------------------------------------------------------------------------
// Stop
//------------------------------------------------------------------------------

void LogicServer::stop() {
    running_ = false;
    if (poll_thread_.joinable())      poll_thread_.join();
    if (broadcast_thread_.joinable()) broadcast_thread_.join();

    {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        for (auto& [fd, _] : clients_) ::close(fd);
        clients_.clear();
    }

    if (epoll_fd_  >= 0) ::close(epoll_fd_);
    if (server_fd_ >= 0) ::close(server_fd_);
    epoll_fd_ = server_fd_ = -1;
}

//------------------------------------------------------------------------------
// Polling loop
//------------------------------------------------------------------------------

void LogicServer::polling_loop() {
    uint64_t period_ns = 1000000000ULL / config_.sample_rate_hz;
    gpio_.read_all();
    auto next = std::chrono::steady_clock::now();

    while (running_) {
        // Si estamos pausados (STOP), no leer GPIO ni llenar buffer
        // para ahorrar CPU. Dormir 50ms y verificar de nuevo.
        if (paused_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            next = std::chrono::steady_clock::now();
            continue;
        }

        next += std::chrono::nanoseconds(period_ns);

        uint32_t states = gpio_.read_all();
        uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        buffer_.push(ts, states);

        auto now = std::chrono::steady_clock::now();
        while (now < next) {
            std::this_thread::yield();
            now = std::chrono::steady_clock::now();
        }
    }
}

//------------------------------------------------------------------------------
// Broadcast loop
//------------------------------------------------------------------------------

void LogicServer::broadcast_loop() {
    std::string pj = pins_json();

    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(SEND_INTERVAL_MS));

        // Si hay solicitud de single-shot, la procesamos
        bool single_mode = single_request_.exchange(false, std::memory_order_acquire);

        // PRIMERO verificar si estamos pausados (antes de drain())
        // Asi los datos se acumulan en el buffer mientras estamos en STOP
        // y cuando se reanuda (RUN) estan disponibles inmediatamente
        if (paused_.load(std::memory_order_acquire) && !single_mode) {
            continue;
        }

        auto samples = buffer_.drain();
        if (samples.empty()) {
            continue;  // esperar datos, preservar pending_reset_
        }

        // Limit samples per WebSocket message to prevent browser issues
        // with very large JSON payloads (~100KB for 3000+ samples).
        // Browser crashes/WebSocket disconnects when receiving too many
        // samples in a single frame. Limiting to 1024 keeps each frame
        // under ~35KB which is safe for all browsers.
        constexpr size_t MAX_SAMPLES_PER_BURST = 1024;
        if (samples.size() > MAX_SAMPLES_PER_BURST) {
            samples.erase(samples.begin(),
                          samples.begin() + (samples.size() - MAX_SAMPLES_PER_BURST));
        }

        bool need_reset = pending_reset_.exchange(false, std::memory_order_acq_rel);

        int trig_idx = trigger_find_index(samples, trigger_);
        std::string json = proto_build_waveform(
            samples, pj, config_.sample_rate_hz, trig_idx, need_reset);
        std::string frame = ws_encode_text(json);

        std::lock_guard<std::mutex> lk(clients_mutex_);
        for (auto& [fd, st] : clients_) {
            if (st.state == ClientState::WS_CONNECTED) {
                bool was_empty = st.write_buf.empty();
                st.write_buf += frame;
                if (was_empty) set_writable(fd);
            }
        }

        // Si era un single-shot, pausamos despues de enviar
        if (single_mode) {
            paused_.store(true, std::memory_order_release);
            LOG_INFO("Server", "Single-shot complete, paused");
        }
    }
}

//------------------------------------------------------------------------------
// Main epoll loop
//------------------------------------------------------------------------------

bool LogicServer::main_loop() {
    struct epoll_event events[MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
        if (n < 0) break;

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (ev & (EPOLLERR | EPOLLHUP)) {
                LOG_WARN("Server", "EPOLLERR/HUP fd=%d events=0x%x", fd, ev);
                close_client(fd);
                continue;
            }
            if (fd == server_fd_) {
                accept_new_client();
            } else {
                if (ev & EPOLLOUT) handle_client_write(fd);
                if (ev & EPOLLIN)  handle_client_read(fd);
            }
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// Accept
//------------------------------------------------------------------------------

void LogicServer::accept_new_client() {
    struct sockaddr_in ca;
    socklen_t al = sizeof(ca);
    int fd = accept4(server_fd_, (struct sockaddr*)&ca, &al, SOCK_NONBLOCK);
    if (fd < 0) return;

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ca.sin_addr, ip, sizeof(ip));
    LOG_INFO("Server", "New connection: %s:%d fd=%d", ip, ntohs(ca.sin_port), fd);

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

    std::lock_guard<std::mutex> lk(clients_mutex_);
    clients_[fd] = ClientState{};
}

//------------------------------------------------------------------------------
// Client read
//------------------------------------------------------------------------------

void LogicServer::handle_client_read(int fd) {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    auto it = clients_.find(fd);
    if (it == clients_.end()) {
        LOG_WARN("Server", "handle_client_read fd=%d not found", fd);
        return;
    }

    auto& c = it->second;
    if (c.state == ClientState::CLOSED) return;

    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf));
    LOG_INFO("Server", "handle_client_read fd=%d n=%ld errno=%d", fd, (long)n, errno);
    if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        close_client_locked(fd, it);
        return;
    }

    if (c.state == ClientState::HTTP_EXPECT) {
        c.read_buf.append(buf, n);
        LOG_INFO("Server", "HTTP_EXPECT buf size=%zu, searching CRLFCRLF", c.read_buf.size());
        size_t he = c.read_buf.find("\r\n\r\n");
        if (he != std::string::npos) {
            LOG_INFO("Server", "Found CRLFCRLF, calling handle_http");
            handle_http(fd, c, it);
            LOG_INFO("Server", "handle_http returned, state=%d write_buf.size=%zu", (int)c.state, c.write_buf.size());
        } else {
            LOG_INFO("Server", "CRLFCRLF NOT found yet");
        }
    } else if (c.state == ClientState::WS_CONNECTED) {
        c.frame_buf.insert(c.frame_buf.end(), buf, buf + n);
        size_t off = 0;
        while (off < c.frame_buf.size()) {
            WS_Frame f;
            if (!ws_decode(c.frame_buf.data() + off,
                           c.frame_buf.size() - off, f)) break;
            handle_ws_frame(fd, f);
            off += f.frame_len;
        }
        if (off > 0)
            c.frame_buf.erase(c.frame_buf.begin(),
                              c.frame_buf.begin() + off);
    }
}

//------------------------------------------------------------------------------
// HTTP handler
//------------------------------------------------------------------------------

void LogicServer::handle_http(int fd, ClientState& c,
                              std::map<int, ClientState>::iterator& it) {
    LOG_INFO("Server", "handle_http ENTER method=? ws_up check");
    const std::string& req = c.read_buf;
    std::istringstream iss(req);
    std::string method, path, ver;
    iss >> method >> path >> ver;
    LOG_INFO("Server", "handle_http method=%s path=%s", method.c_str(), path.c_str());

    if (method != "GET") {
        std::string r = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length:0\r\n\r\n";
        if (write(fd, r.data(), r.size()) < 0) { /* client gone */ }
        close_client_locked(fd, it);
        return;
    }

    // WebSocket upgrade?
    bool ws_up = (req.find("Upgrade: websocket") != std::string::npos) ||
                 (req.find("upgrade: websocket")  != std::string::npos);

    LOG_INFO("Server", "ws_up=%d", ws_up);
    if (ws_up) {
        // Extract Sec-WebSocket-Key
        size_t kp = req.find("Sec-WebSocket-Key:");
        if (kp == std::string::npos)
            kp = req.find("sec-websocket-key:");
        if (kp == std::string::npos)
            kp = req.find("SEC-WEBSOCKET-KEY:");
        LOG_INFO("Server", "key pos=%zu", kp);
        if (kp != std::string::npos) {
            kp = req.find(':', kp) + 1;
            while (kp < req.size() && req[kp] == ' ') kp++;
            // Buscar fin de linea: \r\n, \n, o \r
            size_t ke = req.find('\r', kp);
            size_t ke2 = req.find('\n', kp);
            if (ke == std::string::npos || (ke2 != std::string::npos && ke2 < ke)) ke = ke2;
            if (ke == std::string::npos) ke = req.size();
            c.ws_key = req.substr(kp, ke - kp);
            // Trim trailing whitespace
            while (!c.ws_key.empty() && (c.ws_key.back() == ' ' || c.ws_key.back() == '\r' || c.ws_key.back() == '\n'))
                c.ws_key.pop_back();
            LOG_INFO("Server", "ws_key=%s", c.ws_key.c_str());
        }

        LOG_INFO("Server", "computing accept key...");
        std::string accept = ws_compute_accept_key(c.ws_key);
        LOG_INFO("Server", "accept=%s", accept.c_str());
        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        LOG_INFO("Server", "queuing 101 response...");
        c.write_buf += resp;
        c.write_buf += ws_encode_text(proto_build_state(
            config_.sample_rate_hz, pins_json(), config_.buffer_size));
        // Enviar configuracion persistida al cliente
        {
            std::string ep_json = "[";
            for (size_t i = 0; i < config_.enabled_pins.size(); i++) {
                if (i > 0) ep_json += ",";
                ep_json += std::to_string(config_.enabled_pins[i]);
            }
            ep_json += "]";
            // Zoom/Pan
            float zl = config_.zoom_level;
            float px = config_.pan_x;
            // Decoder config: usar string guardado o "null"
            std::string dec_json = config_.decoder_config_json.empty() ? "null" : config_.decoder_config_json;
            c.write_buf += ws_encode_text(proto_build_config(
                config_.timebase_us, config_.trigger_pin,
                config_.trigger_type,
                proto_build_labels_json(config_.channel_labels),
                ep_json,
                dec_json,
                pins_json(),
                zl, px));
        }
        c.state = ClientState::WS_CONNECTED;
        c.read_buf.clear();
        set_writable(fd);
        LOG_INFO("Server", "101 queued, state+config queued, ws connected");

        LOG_INFO("Server", "Client %d upgraded to WebSocket", fd);
    } else {
        c.write_buf = "";
        serve_html(fd);
        c.state = ClientState::HTTP_DONE;
        set_writable(fd);
    }
}

//------------------------------------------------------------------------------
// WebSocket frame handler
//------------------------------------------------------------------------------

void LogicServer::handle_ws_frame(int fd, const WS_Frame& frame) {
    if (frame.opcode == WS_CLOSE) {
        if (write(fd, ws_encode_close().data(), ws_encode_close().size()) < 0) { /* ignore */ }
        close_client(fd);
    } else if (frame.opcode == WS_PING) {
        std::string pong = ws_encode_pong(frame.payload);
        if (write(fd, pong.data(), pong.size()) < 0) { /* ignore */ }
    } else if (frame.opcode == WS_TEXT) {
        const std::string& cmd = frame.payload;
        if (cmd.find("\"set_trigger\"") != std::string::npos) {
            int pin  = proto_extract_int(cmd, "pin", -1);
            std::string type = proto_extract_string(cmd, "type");
            trigger_.pin  = pin;
            trigger_.type = TriggerConfig::from_string(type);
            // Persistir en config y guardar
            config_.trigger_pin  = pin;
            config_.trigger_type = type;
            config_save_file(config_);
            LOG_INFO("Trigger", "GPIO%d %s — config saved", pin, type.c_str());
        } else if (cmd.find("\"set_timebase\"") != std::string::npos) {
            int value_us = proto_extract_int(cmd, "value_us", 500000);
            config_.timebase_us = value_us;
            config_save_file(config_);
            LOG_INFO("Server", "Timebase set to %d us/div — config saved", value_us);
        } else if (cmd.find("\"set_labels\"") != std::string::npos) {
            // Parse labels from JSON: {"cmd":"set_labels","labels":{"2":"CLK","3":"DATA"}}
            config_.channel_labels.clear();
            size_t pos = cmd.find("\"labels\"");
            if (pos != std::string::npos) {
                pos = cmd.find('{', pos);
                if (pos != std::string::npos) {
                    pos++; // skip {
                    while (pos < cmd.size()) {
                        while (pos < cmd.size() && (cmd[pos]==' '||cmd[pos]=='\t')) pos++;
                        if (pos >= cmd.size() || cmd[pos] == '}') break;
                        if (cmd[pos] != '"') break;
                        pos++;
                        std::string key;
                        while (pos < cmd.size() && cmd[pos] != '"') { key += cmd[pos]; pos++; }
                        if (pos >= cmd.size()) break;
                        pos++;
                        while (pos < cmd.size() && cmd[pos] != ':') pos++;
                        if (pos >= cmd.size()) break;
                        pos++;
                        while (pos < cmd.size() && (cmd[pos]==' '||cmd[pos]=='\t')) pos++;
                        if (pos >= cmd.size() || cmd[pos] != '"') break;
                        pos++;
                        std::string val;
                        while (pos < cmd.size() && cmd[pos] != '"') { val += cmd[pos]; pos++; }
                        if (pos >= cmd.size()) break;
                        pos++;
                        int pin = atoi(key.c_str());
                        if (pin > 0) config_.channel_labels[pin] = val;
                        while (pos < cmd.size() && cmd[pos] != ',' && cmd[pos] != '}') pos++;
                        if (pos < cmd.size() && cmd[pos] == ',') pos++;
                    }
                }
            }
            config_save_file(config_);
            LOG_INFO("Server", "Labels updated (%zu entries) — config saved", config_.channel_labels.size());
        } else if (cmd.find("\"set_enabled_pins\"") != std::string::npos) {
            // Parse enabled pins JSON array: {"cmd":"set_enabled_pins","pins":[2,3,4,5]}
            config_.enabled_pins.clear();
            size_t pos = cmd.find("\"pins\"");
            if (pos != std::string::npos) {
                pos = cmd.find('[', pos);
                if (pos != std::string::npos) {
                    pos++;
                    while (pos < cmd.size() && cmd[pos] != ']') {
                        while (pos < cmd.size() && (cmd[pos]==' '||cmd[pos]=='\t')) pos++;
                        if (pos >= cmd.size() || cmd[pos] == ']') break;
                        if (cmd[pos] >= '0' && cmd[pos] <= '9') {
                            int val = 0;
                            while (pos < cmd.size() && cmd[pos] >= '0' && cmd[pos] <= '9') {
                                val = val * 10 + (cmd[pos] - '0');
                                pos++;
                            }
                            config_.enabled_pins.push_back(val);
                        } else pos++;
                        while (pos < cmd.size() && cmd[pos] == ',') pos++;
                    }
                }
            }
            config_save_file(config_);
            LOG_INFO("Server", "Enabled pins updated (%zu enabled)", config_.enabled_pins.size());
        } else if (cmd.find("\"set_decoder\"") != std::string::npos) {
            // Extraer el objeto decoder completo del comando
            // Formato: {"cmd":"set_decoder","config":{...objeto decoder...}}
            config_.decoder_config_json = "";
            size_t pos = cmd.find("\"config\"");
            if (pos != std::string::npos) {
                pos = cmd.find('{', pos);
                if (pos != std::string::npos) {
                    int depth = 0;
                    size_t start = pos;
                    while (pos < cmd.size()) {
                        if (cmd[pos] == '{') depth++;
                        else if (cmd[pos] == '}') { depth--; if (depth == 0) { pos++; break; } }
                        pos++;
                    }
                    config_.decoder_config_json = cmd.substr(start, pos - start);
                }
            }
            config_save_file(config_);
            LOG_INFO("Server", "Decoder config saved");
        } else if (cmd.find("\"save_config\"") != std::string::npos) {
            LOG_INFO("Server", "Saving config to config.json");
            config_save_file(config_);
        } else if (cmd.find("\"run\"") != std::string::npos) {
            LOG_INFO("Server", "Cmd: run — resuming");
            paused_.store(false, std::memory_order_release);
            pending_reset_.store(true, std::memory_order_release);
        } else if (cmd.find("\"stop\"") != std::string::npos) {
            LOG_INFO("Server", "Cmd: stop — pausing");
            paused_.store(true, std::memory_order_release);
            // Limpiar buffers de salida para pausa instantanea
            std::lock_guard<std::mutex> lk(clients_mutex_);
            for (auto& [fd, st] : clients_) {
                st.write_buf.clear();
            }
        } else if (cmd.find("\"single\"") != std::string::npos) {
            LOG_INFO("Server", "Cmd: single — one-shot");
            paused_.store(false, std::memory_order_release);
            single_request_.store(true, std::memory_order_release);
        } else if (cmd.find("\"set_viewport\"") != std::string::npos) {
            // {"cmd":"set_viewport","zoom":1.5,"pan":123.0}
            float zoom = (float)proto_extract_int(cmd, "zoom", 100) / 100.0f;
            float pan  = (float)proto_extract_int(cmd, "pan", 0);
            config_.zoom_level = zoom;
            config_.pan_x = pan;
            config_save_file(config_);
            LOG_INFO("Server", "Viewport: zoom=%.2f pan=%.0f — config saved", zoom, pan);
        }
    }
}

//------------------------------------------------------------------------------
// Client write
//------------------------------------------------------------------------------

void LogicServer::set_writable(int fd) {
    struct epoll_event ev;
    ev.events  = EPOLLIN | EPOLLOUT;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void LogicServer::clear_writable(int fd) {
    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void LogicServer::handle_client_write(int fd) {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;

    auto& c = it->second;
    if (c.write_buf.empty()) {
        clear_writable(fd);
        return;
    }

    ssize_t n = write(fd, c.write_buf.data(), c.write_buf.size());
    LOG_INFO("Server", "handle_client_write fd=%d n=%ld buf.size=%zu state=%d",
             fd, (long)n, c.write_buf.size(), (int)c.state);
    if (n > 0) {
        c.write_buf.erase(0, n);
        if (c.write_buf.empty()) {
            clear_writable(fd);
            if (c.state == ClientState::HTTP_DONE) {
                LOG_INFO("Server", "HTTP_DONE closing client %d", fd);
                close_client_locked(fd, it);
            }
        }
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        close_client_locked(fd, it);
    }
}

//------------------------------------------------------------------------------
// Serve HTML
//------------------------------------------------------------------------------

void LogicServer::serve_html(int fd) {
    auto it = clients_.find(fd);
    if (it == clients_.end()) return;
    auto& c = it->second;
    std::string html = get_html_page();
    char lb[32];
    snprintf(lb, sizeof(lb), "%zu", html.size());

    c.write_buf +=
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::string(lb) + "\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n" + html;
}

//------------------------------------------------------------------------------
// Close client
//------------------------------------------------------------------------------

void LogicServer::close_client(int fd) {
    std::lock_guard<std::mutex> lk(clients_mutex_);
    auto it = clients_.find(fd);
    if (it != clients_.end()) close_client_locked(fd, it);
}

void LogicServer::close_client_locked(int fd,
                                      std::map<int, ClientState>::iterator& it) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    ::close(fd);
    clients_.erase(it);
    LOG_INFO("Server", "Client %d disconnected", fd);
}

//------------------------------------------------------------------------------
// HTML page loader
//------------------------------------------------------------------------------

std::string LogicServer::get_html_page() {
    static std::string cached;
    if (!cached.empty()) return cached;

    // Rutas absolutas primero (cuando se ejecuta via systemd),
    // luego relativas (para desarrollo local).
    const char* paths[] = {
        "/opt/logic-analyzer/server/web/index.html",
        "web/index.html", "../web/index.html", "./web/index.html"
    };
    for (auto p : paths) {
        FILE* f = fopen(p, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            cached.resize(len);
            if (fread(&cached[0], 1, len, f) != (size_t)len) {
                LOG_WARN("Server", "Failed to read HTML: %s (expected %ld bytes)", p, len);
            }
            fclose(f);
            LOG_INFO("Server", "Loaded HTML: %s (%ld bytes)", p, len);
            return cached;
        }
    }
    LOG_WARN("Server", "web/index.html not found");
    return "<html><body style='background:#111;color:#eee;font-family:sans-serif;"
           "display:flex;align-items:center;justify-content:center;height:100vh;'>"
           "<h1>Logic Analyzer</h1><p>web/index.html not found</p></body></html>";
}

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

std::string LogicServer::pins_json() const {
    std::string r = "[";
    for (int i = 0; i < MAX_GPIO_PINS; i++) {
        if (i > 0) r += ",";
        r += std::to_string(ALL_GPIO_PINS[i]);
    }
    r += "]";
    return r;
}

std::string LogicServer::version_string() const {
    return "Logic Analyzer Server v" LOGIC_VERSION_STRING;
}
