//==============================================================================
// Logic Analyzer Server - C++ mmap GPIO + HTTP + WebSocket
//
// Reads up to 26 GPIO pins via direct memory access (mmap) on Raspberry Pi,
// or generates simulation data on non-Pi systems. Streams pin states to a
// web-based logic analyzer UI via WebSocket.
//
// Usage:
//   ./logic_server [port]          # default port: 8080
//
// Build:
//   make
//
// Architecture:
//   - ARM/ARM64 (Raspberry Pi):   mmap /dev/gpiomem for ~5 MSps reads
//   - x86_64 (development):       simulation mode with configurable patterns
//==============================================================================

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#if GPIO_MMAP
#include <sys/mman.h>
#endif

//==============================================================================
// Constants
//==============================================================================

static constexpr int    MAX_EVENTS        = 64;
static constexpr int    BUFFER_SIZE       = 4096;
static constexpr int    SEND_INTERVAL_MS  = 33;        // ~30 fps
static constexpr int    MAX_GPIO_PINS     = 26;
static constexpr int    DEFAULT_RATE_HZ   = 500000;    // 500 kHz default

static const char*      WS_MAGIC_STRING  = "258EAFA5-E914-47DA-95CA-5AB9DC11B85B";

static const int ALL_GPIO_PINS[MAX_GPIO_PINS] = {
    2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27
};

//==============================================================================
// SHA-1 (RFC 3174) — for WebSocket handshake
//==============================================================================

struct SHA1 {
    uint32_t state[5];
    uint64_t count;
    uint8_t  buffer[64];

    SHA1() { reset(); }

    void reset() {
        state[0] = 0x67452301;
        state[1] = 0xEFCDAB89;
        state[2] = 0x98BADCFE;
        state[3] = 0x10325476;
        state[4] = 0xC3D2E1F0;
        count = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    void update(const uint8_t* data, size_t len) {
        size_t idx = (size_t)(count & 63);
        count += len;
        size_t free = 64 - idx;
        if (len >= free) {
            memcpy(buffer + idx, data, free);
            transform(buffer);
            data += free; len -= free;
            while (len >= 64) { transform(data); data += 64; len -= 64; }
            idx = 0;
        }
        memcpy(buffer + idx, data, len);
    }

    void final(uint8_t digest[20]) {
        uint64_t bits = count * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        while ((count & 63) != 56) update(&pad, 0);
        for (int i = 0; i < 8; i++) update(&((uint8_t*)&bits)[7 - i], 1);
        for (int i = 0; i < 5; i++) {
            digest[i*4+0] = (uint8_t)(state[i] >> 24);
            digest[i*4+1] = (uint8_t)(state[i] >> 16);
            digest[i*4+2] = (uint8_t)(state[i] >> 8);
            digest[i*4+3] = (uint8_t)(state[i]);
        }
    }

    void transform(const uint8_t* block) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
        for (int i = 16; i < 80; i++)
            w[i] = rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i<20)       { f=(b&c)|(~b&d); k=0x5A827999; }
            else if (i<40)  { f=b^c^d;        k=0x6ED9EBA1; }
            else if (i<60)  { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else            { f=b^c^d;        k=0xCA62C1D6; }
            uint32_t temp = rotl(a,5)+f+e+k+w[i];
            e=d;d=c;c=rotl(b,30);b=a;a=temp;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;
    }
    static uint32_t rotl(uint32_t x, uint32_t n) { return (x<<n)|(x>>(32-n)); }
};

//==============================================================================
// Base64 — for WebSocket handshake
//==============================================================================

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string r; r.reserve((len+2)/3*4);
    for (size_t i=0; i<len; i+=3) {
        uint32_t v = (uint32_t)data[i]<<16;
        if (i+1<len) v |= (uint32_t)data[i+1]<<8;
        if (i+2<len) v |= data[i+2];
        r += B64[(v>>18)&0x3F];
        r += B64[(v>>12)&0x3F];
        r += (i+1<len) ? B64[(v>>6)&0x3F] : '=';
        r += (i+2<len) ? B64[v&0x3F] : '=';
    }
    return r;
}

//==============================================================================
// WebSocket Helpers (RFC 6455)
//==============================================================================

enum WS_Opcode { WS_CONTINUATION=0x0, WS_TEXT=0x1, WS_BINARY=0x2,
                 WS_CLOSE=0x8, WS_PING=0x9, WS_PONG=0xA };

static std::string ws_encode_text(const std::string& payload) {
    size_t len = payload.size();
    std::string f; f.reserve(len+10);
    f += (char)(0x80 | WS_TEXT);
    if (len < 126) {
        f += (char)len;
    } else if (len <= 0xFFFF) {
        f += (char)126;
        f += (char)((len>>8)&0xFF); f += (char)(len&0xFF);
    } else {
        f += (char)127;
        for (int i=7; i>=0; i--) f += (char)((len>>(i*8))&0xFF);
    }
    f += payload;
    return f;
}

struct WS_Frame {
    bool fin; uint8_t opcode; bool mask; uint8_t key[4];
    size_t payload_len, frame_len;
    std::string payload;
};

static bool ws_decode(const uint8_t* data, size_t len, WS_Frame& f) {
    if (len < 2) return false;
    f.fin=(data[0]&0x80)!=0; f.opcode=data[0]&0x0F;
    f.mask=(data[1]&0x80)!=0; f.payload_len=data[1]&0x7F;
    size_t off = 2;
    if (f.payload_len==126) {
        if (len<4) return false;
        f.payload_len=((size_t)data[2]<<8)|data[3]; off=4;
    } else if (f.payload_len==127) {
        if (len<10) return false;
        f.payload_len=0;
        for (int i=0;i<8;i++) f.payload_len=(f.payload_len<<8)|data[2+i];
        off=10;
    }
    if (f.mask) { if (len<off+4) return false; memcpy(f.key,data+off,4); off+=4; }
    if (len<off+f.payload_len) return false;
    f.payload.assign((const char*)(data+off), f.payload_len);
    if (f.mask) for (size_t i=0;i<f.payload_len;i++) f.payload[i] ^= f.key[i%4];
    f.frame_len = off + f.payload_len;
    return true;
}

//==============================================================================
// GPIO Reader
//==============================================================================

class GPIOReader {
public:
    GPIOReader() : fd_(-1), gpio_map_(nullptr) {}
    ~GPIOReader() { close(); }

    bool init() {
#if GPIO_MMAP
        fd_ = open("/dev/gpiomem", O_RDWR | O_SYNC);
        if (fd_ < 0) {
            std::cerr << "[GPIO] Failed to open /dev/gpiomem: " << strerror(errno) << std::endl;
            std::cerr << "[GPIO] Falling back to simulation mode" << std::endl;
            return init_simulation();
        }
        gpio_map_ = (volatile uint32_t*)mmap(nullptr, 0x1000, PROT_READ|PROT_WRITE,
                                             MAP_SHARED, fd_, 0);
        if (gpio_map_ == MAP_FAILED) {
            std::cerr << "[GPIO] mmap failed: " << strerror(errno) << std::endl;
            ::close(fd_); fd_ = -1;
            return init_simulation();
        }
        std::cout << "[GPIO] mmap OK — registers at " << (void*)gpio_map_ << std::endl;
        for (int p=0; p<28; p++) {
            int reg=p/10, bit=(p%10)*3;
            gpio_map_[reg] &= ~(7<<bit);
        }
        return true;
#else
        return init_simulation();
#endif
    }

    uint32_t read_all() {
#if GPIO_MMAP
        if (gpio_map_) return gpio_map_[13]; // GPLEV0
#endif
        return simulate_read();
    }

    bool is_simulation() const { return simulation_mode_; }

private:
    bool init_simulation() {
        simulation_mode_=true; sim_counter_=0;
        std::cout << "[GPIO] Simulation mode — generating test patterns" << std::endl;
        return true;
    }

    uint32_t simulate_read() {
        sim_counter_++;
        uint32_t v = 0;
        if ((sim_counter_/250)%2==0)   v |= (1<<2);   // 1 MHz
        if ((sim_counter_/500)%2==0)   v |= (1<<3);   // 500 kHz
        if ((sim_counter_%7)<4)        v |= (1<<4);   // random
        if ((sim_counter_%2000)<1500)  v |= (1<<5);   // 75% duty
        if ((sim_counter_/1000)%2==0)  v |= (1<<6);   // 250 kHz
        if ((sim_counter_/2500)%2==0)  v |= (1<<7);   // 100 kHz
        v |= (1<<8);                                    // always high
        if ((sim_counter_%10)<5)       v |= (1<<10);  // UART-like
        if ((sim_counter_%1000)<500)   v |= (1<<11);  // 50% duty
        if ((sim_counter_/100)%2==0)   v |= (1<<17);  // SPI CLK
        if (((sim_counter_/200)%8)&1)  v |= (1<<22);  // SPI MOSI
        if ((sim_counter_/5000)%5000<4900) v |= (1<<23); // SPI CS
        if ((sim_counter_/600)%2==0)   v |= (1<<24);  // I2C SDA
        if ((sim_counter_%600)<300)    v |= (1<<27);  // I2C SCL
        if ((sim_counter_%4340)<2170)  v |= (1<<14);  // UART TX
        if ((sim_counter_%4340)<2170)  v |= (1<<15);  // UART RX
        return v;
    }

    void close() {
#if GPIO_MMAP
        if (gpio_map_ && gpio_map_!=MAP_FAILED) munmap((void*)gpio_map_, 0x1000);
        if (fd_>=0) ::close(fd_);
#endif
        fd_=-1; gpio_map_=nullptr;
    }

    int fd_=-1;
    volatile uint32_t* gpio_map_=nullptr;
    bool simulation_mode_=false;
    uint64_t sim_counter_=0;
};

//==============================================================================
// Lock-free Ring Buffer (SPSC)
//==============================================================================

struct Sample { uint64_t timestamp_ns; uint32_t gpio_state; };

class RingBuffer {
public:
    RingBuffer(size_t cap = BUFFER_SIZE) : capacity_(cap), write_idx_(0) {
        buffer_.resize(cap);
    }

    void push(uint64_t ts, uint32_t state) {
        size_t next = (write_idx_+1)%capacity_;
        if (next == read_idx_.load(std::memory_order_acquire))
            read_idx_.store((read_idx_.load(std::memory_order_relaxed)+1)%capacity_,
                            std::memory_order_release);
        buffer_[write_idx_] = {ts, state};
        write_idx_ = next;
    }

    std::vector<Sample> drain() {
        std::vector<Sample> r;
        size_t rd = read_idx_.load(std::memory_order_acquire);
        size_t wr = write_idx_;
        if (rd==wr) return r;
        if (wr>rd) { r.reserve(wr-rd);
            r.insert(r.end(),buffer_.begin()+rd,buffer_.begin()+wr); }
        else { r.reserve(capacity_-rd+wr);
            r.insert(r.end(),buffer_.begin()+rd,buffer_.end());
            r.insert(r.end(),buffer_.begin(),buffer_.begin()+wr); }
        read_idx_.store(wr, std::memory_order_release);
        return r;
    }

    size_t size() const {
        size_t r=read_idx_.load(std::memory_order_acquire), w=write_idx_;
        return (w>=r) ? w-r : capacity_-r+w;
    }

private:
    size_t capacity_, write_idx_;
    std::atomic<size_t> read_idx_{0};
    std::vector<Sample> buffer_;
};

//==============================================================================
// Trigger config (shared state)
//==============================================================================

struct TriggerConfig {
    int      pin  = -1;    // -1 = disabled
    std::string type = "rising";
};

//==============================================================================
// HTTP / WebSocket Server
//==============================================================================

class LogicServer {
public:
    LogicServer(int port)
        : port_(port), server_fd_(-1), epoll_fd_(-1), running_(false),
          sample_rate_(DEFAULT_RATE_HZ) {}
    ~LogicServer() { stop(); }

    bool start() {
        if (!gpio_.init()) return false;

        server_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (server_fd_<0) { perror("socket"); return false; }

        int opt=1;
        setsockopt(server_fd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
        setsockopt(server_fd_,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY;
        addr.sin_port=htons(port_);
        if (bind(server_fd_,(struct sockaddr*)&addr,sizeof(addr))<0) {
            perror("bind"); return false;
        }
        if (listen(server_fd_,16)<0) { perror("listen"); return false; }

        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_<0) { perror("epoll"); return false; }

        struct epoll_event ev;
        ev.events=EPOLLIN; ev.data.fd=server_fd_;
        epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,server_fd_,&ev);

        running_=true;
        poll_thread_ = std::thread(&LogicServer::polling_loop,this);
        broadcast_thread_ = std::thread(&LogicServer::broadcast_loop,this);

        std::cout << "\n"
            "  +=============================================+\n"
            "  |      LOGIC ANALYZER SERVER v1.0             |\n"
            "  +=============================================+\n"
            "  |  Port:    " << port_ << "                              |\n"
            "  |  URL:     http://localhost:" << port_ << "        |\n"
            "  |  Pins:    26 (GPIO 2-27)                 |\n"
            "  |  Rate:    " << (sample_rate_/1000) << " kHz                    |\n"
            "  |  Mode:    " << (gpio_.is_simulation()?"SIMULATION":"HW GPIO mmap") << "         |\n"
            "  +=============================================+\n" << std::endl;

        return main_loop();
    }

    void stop() {
        running_=false;
        if (poll_thread_.joinable()) poll_thread_.join();
        if (broadcast_thread_.joinable()) broadcast_thread_.join();
        { std::lock_guard<std::mutex> lk(clients_mutex_);
          for (auto& [fd,_] : clients_) ::close(fd);
          clients_.clear(); }
        if (epoll_fd_>=0) ::close(epoll_fd_);
        if (server_fd_>=0) ::close(server_fd_);
        epoll_fd_=-1; server_fd_=-1;
    }

private:
    struct ClientState {
        enum State { HTTP_EXPECT, WS_CONNECTED, CLOSED };
        State state=HTTP_EXPECT;
        std::string read_buf, write_buf, ws_key;
        std::vector<uint8_t> frame_buf;
    };

    //----------------------------------------------------------------------
    // Polling loop: busy-spin for accurate timing
    //----------------------------------------------------------------------
    void polling_loop() {
        uint64_t period_ns = 1000000000ULL / sample_rate_;
        gpio_.read_all();
        auto next = std::chrono::steady_clock::now();

        while (running_) {
            next += std::chrono::nanoseconds(period_ns);

            uint32_t states = gpio_.read_all();
            uint64_t ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            buffer_.push(ts, states);

            // Busy-spin wait (accurate to ~100ns)
            auto now = std::chrono::steady_clock::now();
            while (now < next) {
                std::this_thread::yield();
                now = std::chrono::steady_clock::now();
            }
        }
    }

    //----------------------------------------------------------------------
    // Broadcast loop: send data to all WebSocket clients
    //----------------------------------------------------------------------
    void broadcast_loop() {
        std::string pins_json = "[";
        for (int i=0; i<MAX_GPIO_PINS; i++) {
            if (i>0) pins_json += ",";
            pins_json += std::to_string(ALL_GPIO_PINS[i]);
        }
        pins_json += "]";
        std::string rate_str = std::to_string(sample_rate_);

        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SEND_INTERVAL_MS));

            auto samples = buffer_.drain();
            if (samples.empty()) continue;

            // Check trigger patterns if enabled
            int trig_idx = -1;
            if (trigger_.pin >= 0 && samples.size() > 2) {
                int last_state = (samples[0].gpio_state >> trigger_.pin) & 1;
                for (size_t i=1; i<samples.size(); i++) {
                    int cur = (samples[i].gpio_state >> trigger_.pin) & 1;
                    bool match = false;
                    if (trigger_.type=="rising" && last_state==0 && cur==1) match=true;
                    else if (trigger_.type=="falling" && last_state==1 && cur==0) match=true;
                    else if (trigger_.type=="both" && last_state!=cur) match=true;
                    else if (trigger_.type=="high" && cur==1) match=true;
                    else if (trigger_.type=="low" && cur==0) match=true;
                    if (match) { trig_idx=i; break; }
                    last_state = cur;
                }
            }

            // Build JSON
            std::string json = "{\"type\":\"waveform\",\"pins\":" + pins_json + ",\"timestamps\":[";
            for (size_t i=0; i<samples.size(); i++) {
                if (i>0) json += ",";
                json += std::to_string(samples[i].timestamp_ns);
            }
            json += "],\"states\":[";
            for (size_t i=0; i<samples.size(); i++) {
                if (i>0) json += ",";
                json += std::to_string(samples[i].gpio_state);
            }
            json += "],\"t0\":" + std::to_string(samples[0].timestamp_ns);
            json += ",\"dt_us\":1";
            json += ",\"rate\":" + rate_str;
            json += ",\"samples\":" + std::to_string(samples.size());
            json += ",\"trigger_index\":" + std::to_string(trig_idx);
            json += "}";

            std::string frame = ws_encode_text(json);

            std::lock_guard<std::mutex> lk(clients_mutex_);
            for (auto& [fd,st] : clients_)
                if (st.state==ClientState::WS_CONNECTED) st.write_buf += frame;
        }
    }

    //----------------------------------------------------------------------
    // Main epoll loop
    //----------------------------------------------------------------------
    bool main_loop() {
        struct epoll_event events[MAX_EVENTS];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
            if (n<0 && errno!=EINTR) break;
            for (int i=0; i<n; i++) {
                int fd=events[i].data.fd;
                if (events[i].events&(EPOLLERR|EPOLLHUP)) { close_client(fd); continue; }
                if (fd==server_fd_) accept_new_client();
                else {
                    if (events[i].events&EPOLLIN) handle_client_read(fd);
                    if (events[i].events&EPOLLOUT) handle_client_write(fd);
                }
            }
        }
        return true;
    }

    void accept_new_client() {
        struct sockaddr_in ca; socklen_t al=sizeof(ca);
        int fd = accept4(server_fd_,(struct sockaddr*)&ca,&al,SOCK_NONBLOCK);
        if (fd<0) return;
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET,&ca.sin_addr,ip,sizeof(ip));
        std::cout << "[Server] New connection from " << ip << ":" << ntohs(ca.sin_port) << std::endl;
        struct epoll_event ev; ev.events=EPOLLIN|EPOLLOUT; ev.data.fd=fd;
        epoll_ctl(epoll_fd_,EPOLL_CTL_ADD,fd,&ev);
        std::lock_guard<std::mutex> lk(clients_mutex_);
        clients_[fd] = ClientState{};
    }

    void handle_client_read(int fd) {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        auto it = clients_.find(fd);
        if (it==clients_.end()) return;
        auto& c = it->second;
        if (c.state==ClientState::CLOSED) return;

        char buf[8192];
        ssize_t n = read(fd,buf,sizeof(buf));
        if (n<=0) {
            if (n==0||(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK))
                close_client_locked(fd,it);
            return;
        }

        if (c.state==ClientState::HTTP_EXPECT) {
            c.read_buf.append(buf,n);
            size_t he = c.read_buf.find("\r\n\r\n");
            if (he!=std::string::npos) handle_http(fd,c,it);
        } else if (c.state==ClientState::WS_CONNECTED) {
            c.frame_buf.insert(c.frame_buf.end(),buf,buf+n);
            size_t off=0;
            while (off<c.frame_buf.size()) {
                WS_Frame f;
                if (!ws_decode(c.frame_buf.data()+off,c.frame_buf.size()-off,f)) break;
                handle_ws_frame(fd,f);
                off += f.frame_len;
            }
            if (off>0) c.frame_buf.erase(c.frame_buf.begin(),c.frame_buf.begin()+off);
        }
    }

    void handle_http(int fd, ClientState& c, std::map<int,ClientState>::iterator& it) {
        const std::string& req = c.read_buf;
        std::istringstream iss(req);
        std::string method,path,ver;
        iss>>method>>path>>ver;

        if (method=="GET") {
            bool ws_up = (req.find("Upgrade: websocket")!=std::string::npos) ||
                         (req.find("upgrade: websocket")!=std::string::npos);
            if (ws_up) {
                size_t kp = req.find("Sec-WebSocket-Key:");
                if (kp==std::string::npos) kp=req.find("sec-websocket-key:");
                if (kp!=std::string::npos) {
                    kp=req.find(':',kp)+1;
                    while(kp<req.size()&&req[kp]==' ') kp++;
                    size_t ke=req.find('\r',kp);
                    c.ws_key=req.substr(kp,ke-kp);
                }
                std::string concat=c.ws_key+WS_MAGIC_STRING;
                SHA1 sha; sha.update((const uint8_t*)concat.data(),concat.size());
                uint8_t dig[20]; sha.final(dig);
                std::string accept=base64_encode(dig,20);
                std::string resp=
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: "+accept+"\r\n\r\n";
                write(fd,resp.data(),resp.size());
                c.state=ClientState::WS_CONNECTED;
                c.read_buf.clear();
                std::cout << "[Server] Client " << fd << " upgraded to WebSocket" << std::endl;

                // Send init state msg
                std::string sj = "{\"type\":\"state\",\"mode\":\"run\",\"rate\":" +
                    std::to_string(sample_rate_) + ",\"pins\":[";
                for (int i=0;i<MAX_GPIO_PINS;i++) {
                    if(i>0)sj+=","; sj+=std::to_string(ALL_GPIO_PINS[i]);
                }
                sj += "],\"samples\":" + std::to_string(BUFFER_SIZE) + "}";
                c.write_buf += ws_encode_text(sj);
            } else {
                serve_html(fd);
                close_client_locked(fd,it);
            }
        } else {
            std::string r="HTTP/1.1 405 Method Not Allowed\r\nContent-Length:0\r\n\r\n";
            write(fd,r.data(),r.size());
            close_client_locked(fd,it);
        }
    }

    void serve_html(int fd) {
        std::string html = get_html_page();
        char lb[32]; snprintf(lb,sizeof(lb),"%zu",html.size());
        std::string resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: "+std::string(lb)+"\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"+html;
        write(fd,resp.data(),resp.size());
    }

    //----------------------------------------------------------------------
    // WebSocket command handler
    //----------------------------------------------------------------------
    void handle_ws_frame(int fd, const WS_Frame& frame) {
        if (frame.opcode == WS_CLOSE) {
            std::string cf; cf+=(char)(0x80|WS_CLOSE); cf+=(char)0;
            write(fd,cf.data(),cf.size());
            close_client(fd);
        } else if (frame.opcode == WS_PING) {
            std::string pf; pf+=(char)(0x80|WS_PONG);
            if (frame.payload.size()<126) pf+=(char)frame.payload.size();
            else { pf+=(char)126; pf+=(char)((frame.payload.size()>>8)&0xFF);
                   pf+=(char)(frame.payload.size()&0xFF); }
            pf+=frame.payload;
            write(fd,pf.data(),pf.size());
        } else if (frame.opcode == WS_TEXT) {
            // Parse JSON command from client
            const std::string& cmd = frame.payload;
            if (cmd.find("\"cmd\"") != std::string::npos) {
                if (cmd.find("\"set_trigger\"") != std::string::npos) {
                    // Extract trigger pin
                    size_t pp = cmd.find("\"pin\"");
                    if (pp != std::string::npos) {
                        pp = cmd.find(':', pp) + 1;
                        while (pp < cmd.size() && (cmd[pp]==' '||cmd[pp]=='\t')) pp++;
                        trigger_.pin = 0;
                        bool neg = false;
                        if (pp < cmd.size() && cmd[pp]=='-') { neg=true; pp++; }
                        while (pp < cmd.size() && cmd[pp]>='0' && cmd[pp]<='9') {
                            trigger_.pin = trigger_.pin*10 + (cmd[pp]-'0');
                            pp++;
                        }
                        if (neg) trigger_.pin = -trigger_.pin;
                    }
                    // Extract trigger type
                    size_t tp = cmd.find("\"type\"");
                    if (tp != std::string::npos) {
                        tp = cmd.find(':', tp) + 1;
                        while (tp < cmd.size() && (cmd[tp]==' '||cmd[tp]=='"'||cmd[tp]=='\t')) tp++;
                        trigger_.type.clear();
                        while (tp < cmd.size() && cmd[tp]!='"' && cmd[tp]!=',' && cmd[tp]!='}') {
                            trigger_.type += cmd[tp]; tp++;
                        }
                    }
                    std::cout << "[Server] Trigger: GPIO" << trigger_.pin << " " << trigger_.type << std::endl;
                }
                // Other commands logged but not affecting acquisition (always running)
                if (cmd.find("\"run\"") != std::string::npos) {
                    std::cout << "[Server] Cmd: run (always streaming)" << std::endl;
                } else if (cmd.find("\"stop\"") != std::string::npos) {
                    std::cout << "[Server] Cmd: stop (data continues)" << std::endl;
                }
            }
        }
    }

    void handle_client_write(int fd) {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        auto it=clients_.find(fd);
        if (it==clients_.end()) return;
        auto& c=it->second;
        if (c.write_buf.empty()) return;
        ssize_t n=write(fd,c.write_buf.data(),c.write_buf.size());
        if (n>0) c.write_buf.erase(0,n);
        else if (n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK) close_client_locked(fd,it);
    }

    void close_client(int fd) {
        std::lock_guard<std::mutex> lk(clients_mutex_);
        auto it=clients_.find(fd);
        if (it!=clients_.end()) close_client_locked(fd,it);
    }

    void close_client_locked(int fd, std::map<int,ClientState>::iterator& it) {
        epoll_ctl(epoll_fd_,EPOLL_CTL_DEL,fd,nullptr);
        ::close(fd); clients_.erase(it);
        std::cout << "[Server] Client " << fd << " disconnected" << std::endl;
    }

    std::string get_html_page() {
        static std::string cached;
        if (!cached.empty()) return cached;
        const char* paths[] = {"web/index.html","../web/index.html","./web/index.html"};
        for (auto p : paths) {
            FILE* f=fopen(p,"rb");
            if (f) {
                fseek(f,0,SEEK_END); long len=ftell(f); fseek(f,0,SEEK_SET);
                cached.resize(len); fread(&cached[0],1,len,f); fclose(f);
                std::cout << "[Server] Loaded HTML: " << p << " (" << len << " bytes)" << std::endl;
                return cached;
            }
        }
        return "<html><body style='background:#111;color:#eee;font-family:sans-serif;"
               "display:flex;align-items:center;justify-content:center;height:100vh;'>"
               "<h1>Logic Analyzer</h1><p>web/index.html not found</p></body></html>";
    }

    int port_, server_fd_=-1, epoll_fd_=-1;
    std::atomic<bool> running_{false};
    std::map<int,ClientState> clients_;
    std::mutex clients_mutex_;
    GPIOReader gpio_;
    RingBuffer buffer_;
    std::thread poll_thread_, broadcast_thread_;
    int sample_rate_;
    TriggerConfig trigger_;
};

//==============================================================================
// Entry point
//==============================================================================

static std::atomic<bool> g_shutdown(false);
static void sig_handler(int) { g_shutdown=true; }

int main(int argc, char* argv[]) {
    int port = (argc>1) ? atoi(argv[1]) : 8080;
    if (port<=0||port>65535) port=8080;

    signal(SIGINT,sig_handler); signal(SIGTERM,sig_handler); signal(SIGPIPE,SIG_IGN);

    LogicServer srv(port);
    if (!srv.start()) {
        std::cerr << "[Server] Failed to start on port " << port << std::endl;
        return 1;
    }
    while (!g_shutdown) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "\n[Server] Shutting down..." << std::endl;
    srv.stop();
    std::cout << "[Server] Done." << std::endl;
    return 0;
}
