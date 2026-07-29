# Requisitos Profesionales — Analizador Logico RPi

## 1. Estandares de Codigo

### 1.1 Comentarios

Todo archivo de codigo debe tener:

```cpp
//==============================================================================
// archivo.h / archivo.cpp
// Descripcion: que hace este modulo
// Autor: siliconvalleyar-oss
// Licencia: MIT
//==============================================================================
```

Toda funcion publica debe tener comentario Doxygen/JSDoc:

**C++:**
```cpp
/**
 * Lee el estado actual de todos los pines GPIO configurados.
 * Usa mmap a /dev/gpiomem para lectura en un solo acceso.
 *
 * @param pin_mask  Mascara de bits de los pines a leer (bit 0 = GPIO0)
 * @param buffer    Buffer de salida con los estados leidos
 * @param count     Cantidad de muestras a leer
 * @return          Cantidad de muestras leidas, o -1 en error
 * @throws          std::system_error si el mmap falla
 *
 * @note            Esta funcion NO es segura para usar desde ISR
 * @see             setup_gpio_mmap(), release_gpio_mmap()
 */
int gpio_read_bulk(uint32_t pin_mask, uint32_t* buffer, size_t count);
```

**JavaScript:**
```javascript
/**
 * Decodifica una trama I2C a partir de los estados de SCL y SDA.
 *
 * @param {number[]} timestamps - Array de timestamps en nanosegundos
 * @param {number[]} states     - Array de bitfields (bit 0 = SCL, bit 1 = SDA)
 * @param {object}   config     - Configuracion { scl_pin, sda_pin }
 * @returns {object[]} Array de eventos decodificados:
 *   @returns {number} t        - Timestamp del evento
 *   @returns {string} type     - "start" | "address" | "data" | "ack" | "nack" | "stop"
 *   @returns {string} label    - Texto a mostrar en pantalla
 *   @returns {boolean} ack     - true si hubo ACK
 *
 * @example
 * const items = decodeI2C(ts, states, { scl_pin: 0, sda_pin: 1 });
 * // → [{ t: 100, type: "start", label: "START" }, ...]
 */
function decodeI2C(timestamps, states, config) { ... }
```

**Python:**
```python
def acquire_samples(pins: list[int], rate_hz: int, count: int) -> list[tuple[int, int]]:
    """
    Adquiere muestras de los pines GPIO especificados.

    Args:
        pins:    Lista de numeros de GPIO a muestrear
        rate_hz: Frecuencia de muestreo en Hz
        count:   Cantidad de muestras a adquirir

    Returns:
        Lista de tuplas (timestamp_ns, bitfield) donde bitfield
        tiene el bit i = estado de pins[i]

    Raises:
        ValueError: Si rate_hz > maximo soportado
        RuntimeError: Si gpiod no puede acceder a los pines

    Example:
        >>> samples = acquire_samples([17, 22], 100000, 1024)
        >>> len(samples)
        1024
    """
```

### 1.2 Nombres de variables

| Tipo | Convencion | Ejemplo |
|------|-----------|---------|
| Variables locales | snake_case | `sample_count`, `pin_mask` |
| Funciones | snake_case | `decode_i2c()`, `acquire_samples()` |
| Clases | PascalCase | `LogicAnalyzer`, `WebSocketServer` |
| Constantes | UPPER_SNAKE | `BUFFER_SIZE`, `MAX_PINS` |
| Archivos | kebab-case | `logic-server.cpp`, `web-index.html` |
| HTML IDs | kebab-case | `run-button`, `channel-panel` |
| HTML clases | kebab-case | `waveform-canvas`, `control-bar` |

### 1.3 Longitud maxima

- Lineas: 100 caracteres (C++), 120 (JS/Python/HTML)
- Funciones: max 60 lineas (si es mas, dividir)
- Archivos: max 1000 lineas (si es mas, dividir en modulos)

### 1.4 Errores y logging

**C++:**
```cpp
enum LogLevel { DEBUG, INFO, WARN, ERROR, FATAL };

class Logger {
public:
    static void init(const std::string& filepath);
    static void log(LogLevel level, const std::string& module,
                    const std::string& message);
    static void set_min_level(LogLevel level);
};

#define LOG_DEBUG(mod, msg)  Logger::log(DEBUG, mod, msg)
#define LOG_INFO(mod, msg)   Logger::log(INFO, mod, msg)
#define LOG_WARN(mod, msg)   Logger::log(WARN, mod, msg)
#define LOG_ERROR(mod, msg)  Logger::log(ERROR, mod, msg)
```

Formato de log:
```
[2026-07-28 21:30:00.123] [INFO] [Acquisition] Started sampling at 200000 Hz, 8 pins
[2026-07-28 21:30:00.456] [WARN] [Trigger] Trigger level 3.3V out of range, clipping to 3.3V
[2026-07-28 21:30:01.000] [ERROR] [I2C] Failed to read from /dev/i2c-1: No such device
```

### 1.5 Manejo de errores

- Todo error debe ser capturado y registrado
- Nunca silenciar excepciones (no `catch {}` vacio)
- En C++: usar `std::expected` o `std::optional` para errores esperados
- En JS: usar try/catch con mensajes descriptivos
- En Python: usar logging + excepciones especificas
- Errores de hardware (GPIO, I2C): reintentar 3 veces antes de fallar
- Errores de red (WebSocket): reconexion automatica exponencial

## 2. Arquitectura

### 2.1 Server C++ (server/main.cpp)

```
main()
├── setup_signal_handlers()       // SIGINT, SIGTERM limpios
├── load_config()                  // config.json
├── init_gpio_mmap()              // /dev/gpiomem
├── init_buffer()                  // Ring buffer SPSC
├── start_acquisition_thread()    // SCHED_FIFO prioridad 80
├── start_websocket_server()      // uWebSockets :8091
├── start_http_server()           // uWebSockets :8080
└── event_loop()                   // esperar senales
```

Hilos:
- **Acquisition thread** (prioridad maxima): solo lee GPIO y escribe al ring buffer
- **Worker thread**: lee del ring buffer, detecta trigger, arma mensajes JSON
- **Main thread**: WebSocket + HTTP (uWebSockets event loop)

### 2.2 Web Frontend (web/index.html)

```
index.html (unico archivo, todo inline)
├── CSS (~200 lineas)
│   ├── Reset, variables, tema oscuro
│   ├── Top bar, paneles, canvas
│   ├── Responsive (320px-4K)
│   └── Animaciones minimas
├── HTML (~100 lineas)
│   ├── Top bar (host, connect, status)
│   ├── Control bar (RUN/STOP/SINGLE, timebase)
│   ├── Channel list (nombres, colores, enable)
│   ├── Canvas (waveform digital)
│   ├── Decoder bar (protocol selector, decoded text)
│   ├── Cursor bar (tA, tB, delta, freq)
│   └── Bottom bar (zoom, export, state)
└── JavaScript (~800 lineas)
    ├── WebSocket client (conexion, reconexion, parseo)
    ├── Canvas rendering (grid, waveforms, trigger, cursors)
    ├── LTTB downsampling
    ├── Trigger detection
    ├── Decoders (I2C, UART, SPI, PWM)
    ├── Cursors (A/B, drag, delta)
    ├── Measurements (freq, duty, period)
    ├── Export (CSV, screenshot)
    └── Event handlers (mouse, keyboard, touch)
```

## 3. UI/UX

### 3.1 Layout responsivo

```
>=1024px (desktop):
┌──────────────────────────────────────────────────────┐
│ TOP BAR: [host:____] [Connect] ● CONNECTED 200 kHz   │
├──────────────────┬───────────────────────────────────┤
│ CHANNEL LIST     │                                   │
│ D0 CLK  [x] #FF6B│          CANVAS                   │
│ D1 DATA [x] #4ECD│     (formas digitales)            │
│ D2 CS   [x] #45B7│                                   │
│ D3 MOSI [x] #96CE│                                   │
│ D4 MISO [x] #FFEA│                                   │
│ D5 TX   [x] #DDA0│                                   │
│ D6 RX   [x] #98D8│                                   │
├──────────────────┴───────────────────────────────────┤
│ CONTROL: [▶RUN] [■STOP] [◉SINGLE] [10us/div ▼]       │
│ TRIGGER: [D0▼] [↗rising▼] Level: [1.65V] Pos: [50%]│
├──────────────────────────────────────────────────────┤
│ DECODED: START 0x48 W ACK 0xAB ACK 0xCD ACK STOP    │
│ CURSORS: tA=100us tB=500us Δt=400us f=2.50kHz       │
├──────────────────────────────────────────────────────┤
│ [I2C▼] [Export CSV] [Screenshot] [Zoom+] [Zoom-]     │
└──────────────────────────────────────────────────────┘

<768px (mobile):
┌────────────────────┐
│ [host] [Connect] ● │
├────────────────────┤
│ [▶] [■] [◉] 10us ▼│
├────────────────────┤
│ D0 CLK  ━━┯━━┯━   │
│ D1 DATA ━━┷━━┷━   │
│ D2 CS   ━━┯━━┯━   │
│ ...                │
├────────────────────┤
│ DECODED: START..   │
│ CURSORS: Δt=400us  │
├────────────────────┤
│ [I2C▼] [Export]   │
└────────────────────┘
```

### 3.2 Colores profesionales

| Elemento | Color | Hex |
|----------|-------|-----|
| Fondo principal | Negro azulado | `#0d0d1a` |
| Fondo paneles | Gris oscuro | `#1a1a2e` |
| Fondo inputs | Gris medio | `#2a2a3e` |
| Texto principal | Blanco humo | `#c8d6e5` |
| Texto secundario | Gris claro | `#8395a7` |
| Borde | Gris tenue | `#3a3a4e` |
| Acento azul | Azul profundo | `#3867d6` |
| Exito | Verde | `#26de81` |
| Error | Rojo | `#fc5c65` |
| Warning | Naranja | `#fd9644` |
| Canal 0 | Rojo coral | `#FF6B6B` |
| Canal 1 | Turquesa | `#4ECDC4` |
| Canal 2 | Azul cielo | `#45B7D1` |
| Canal 3 | Verde salvia | `#96CEB4` |
| Canal 4 | Amarillo | `#FFEAA7` |
| Canal 5 | Purple | `#DDA0DD` |
| Canal 6 | Verde menta | `#98D8C8` |
| Canal 7 | Dorado | `#F7DC6F` |

### 3.3 Tipografia

```css
--font-sans: 'Inter', 'Segoe UI', system-ui, -apple-system, sans-serif;
--font-mono: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace;
--font-size-xs: 11px;   // Labels, metadatos
--font-size-sm: 13px;   // Valores, mediciones
--font-size-md: 14px;   // Body, botones
--font-size-lg: 16px;   // Nombres de canal
--font-size-xl: 20px;   // Titulos de panel
--font-size-2xl: 24px;  // Valor destacado
```

### 3.4 Interacciones

- **Rueda**: zoom horizontal (timebase)
- **Ctrl+rueda**: zoom vertical (altura de filas)
- **Click en canvas**: posiciona cursor A (rojo)
- **Shift+click**: posiciona cursor B (azul)
- **Arrastre**: mueve la ventana de tiempo (pan)
- **Doble click**: reset zoom
- **Click en nombre de canal**: editar nombre
- **Click en color de canal**: cambiar color (picker simple)
- **Hover en forma de onda**: tooltip con valor del pin en ese punto
- **Hover en decoded item**: tooltip con detalle completo

### 3.5 Keyboard shortcuts

| Tecla | Accion |
|-------|--------|
| `R` | RUN |
| `S` | STOP |
| `G` | SINGLE (arm trigger) |
| `Space` | RUN/STOP toggle |
| `+` | Zoom in |
| `-` | Zoom out |
| `0` | Reset zoom |
| `C` | Toggle cursors |
| `D` | Toggle decoders panel |
| `E` | Export CSV |
| `F` | Fullscreen |
| `↵` | Connect/Disconnect |
| `?` | Show keyboard shortcuts |

### 3.6 Estados visuales

| Estado | Indicador |
|--------|-----------|
| Desconectado | Circulo rojo + "DISCONNECTED" |
| Conectando | Circulo amarillo + "CONNECTING..." + spinner |
| Conectado (STOP) | Circulo verde + "STOPPED" + ultimo frame |
| Conectado (RUN) | Circulo verde parpadeante + "RUN" + animacion |
| Conectado (SINGLE armed) | Circulo azul + "ARMED" |
| Conectado (SINGLE captured) | Circulo verde fijo + "TRIGGERED" |
| Error | Toast rojo + "ERROR: mensaje" |

## 4. Performance

### 4.1 Server C++

| Requisito | Objetivo | Medicion |
|-----------|----------|----------|
| Sample rate max | 5 MSps | `time between samples < 200ns` |
| Latencia GPIO→WS | < 1ms | `timestamp(enviado) - timestamp(leido)` |
| Clientes simultaneos | 10+ | WebSocket broadcast |
| Buffer loss | 0% en 1 hora | `muestras_enviadas / muestras_leidas = 1.0` |
| CPU usage | < 30% (1 core) | `top -b -n 1` |
| RAM usage | < 64 MB | `ps -o rss` |
| Startup time | < 1s | `time ./logic_server` |

### 4.2 Web Frontend

| Requisito | Objetivo | Medicion |
|-----------|----------|----------|
| Frame rate | 30 fps | `requestAnimationFrame` delta |
| Canvas repaint | < 16ms | Chrome DevTools Performance |
| WebSocket latency | < 50ms | `Date.now() - server_timestamp` |
| JS memory | < 50 MB | `performance.memory.usedJSHeapSize` |
| Page load | < 2s | Lighthouse |
| Downsampling (1024→512) | < 1ms | `console.time()` |
| Decode I2C (1024 samples) | < 5ms | `console.time()` |

## 5. Testing

### 5.1 Server

```bash
# Test de humo: servidor arranca
./logic_server 8080 &
sleep 1 && curl -s http://localhost:8080 | head -5

# Test WebSocket: conexion y datos
websocat ws://localhost:8091 <<< '{"cmd":"run"}'

# Test de carga: 5 clientes simultaneos
for i in {1..5}; do websocat ws://localhost:8091 & done

# Test de larga duracion: 1 hora sin perdida
timeout 3600 ./logic_server 8080
# Verificar logs: buscar "ERROR" o "buffer overflow"

# Test sin GPIO (modo simulacion)
GPIO_SIM=1 ./logic_server 8080
```

### 5.2 Web

```bash
# Pruebas manuales:
# - Abrir en Chrome, Firefox, Safari, Edge
# - Abrir en celular (Android Chrome, iOS Safari)
# - Probar RUN/STOP/SINGLE
# - Probar zoom, pan, cursores
# - Probar cada decodificador con senales conocidas

# Prueba de red:
# - iperf3 entre Pi y cliente (> 10 Mbps)
# - ping < 5ms (misma red)
```

## 6. Seguridad

- No ejecutar como root (usar `gpio` group para /dev/gpiomem)
- El servidor HTTP solo sirve en red local por defecto
- No exponer puertos publicamente sin autenticacion
- WebSocket solo en modo texto (no binario, para debug)
- Timeout de cliente inactivo: 60 segundos
- Rate limiting: max 100 mensajes/segundo por cliente
- Validar todos los JSON entrantes (schema validation)
- Sanitizar nombres de canal (solo alfanumerico + espacio)

## 7. Configuracion

### config.json
```json
{
  "server": {
    "http_port": 8080,
    "ws_port": 8091,
    "bind_address": "0.0.0.0"
  },
  "acquisition": {
    "pins": [17, 22, 23, 24, 27, 4, 5, 6],
    "rate_hz": 200000,
    "buffer_size": 4096,
    "mode": "polling"
  },
  "trigger": {
    "pin": 17,
    "type": "rising",
    "hpos": 50
  },
  "display": {
    "timebase_us": 10,
    "zoom_y": 1.0,
    "theme": "dark"
  },
  "decoders": [
    {"protocol": "i2c", "enabled": true, "scl_pin": 0, "sda_pin": 1},
    {"protocol": "uart", "enabled": false, "rx_pin": 2, "baud": 115200}
  ],
  "logging": {
    "level": "INFO",
    "file": "/var/log/logic-analyzer.log",
    "max_size_mb": 10
  }
}
```

### CLI arguments
```
./logic_server [options]

Options:
  -p, --port <port>       HTTP port (default: 8080)
  -c, --config <file>     Config file (default: config.json)
  -r, --rate <hz>         Sample rate (default: 200000)
  --pins <list>           GPIO pins (comma-separated, default: 17,22,23,24,27)
  --simulate              Simulation mode (no GPIO required)
  -v, --verbose           Verbose logging
  -l, --log <file>        Log file path
  --version               Show version
  --help                  Show help
```

## 8. Documentacion del codigo

### 8.1 README para cada modulo

Cada carpeta debe tener README.md:

```markdown
# server/ — Servidor del Analizador Logico

## Descripcion
Servidor C++ que lee pines GPIO por mmap y sirve datos via WebSocket.

## Dependencias
- Linux con /dev/gpiomem (Raspberry Pi)
- uWebSockets (incluido como header-only)
- g++ >= 10, cmake >= 3.16

## Compilacion
```bash
make
```

## Ejecucion
```bash
./logic_server 8080
```

## Arquitectura
Ver docs/architecture.md

## Tests
```bash
make test
```
```

### 8.2 Changelog

```markdown
# Changelog

## [1.0.0] - 2026-07-28
### Added
- Server C++ con mmap GPIO (5 MSps, 26 canales)
- Web UI con Canvas rendering
- Decodificadores: I2C, UART, SPI, PWM
- Trigger por flanco, cursores A/B
- Export CSV y screenshot
```

### 8.3 Contributing

```markdown
# Contributing

1. Fork el repo
2. Crear branch: `git checkout -b feat/nueva-feature`
3. Commit: `git commit -m "feat: agregar soporte para protocolo CAN bus"`
4. Push: `git push origin feat/nueva-feature`
5. Abrir Pull Request

## Convencion de commits
- `feat:` nueva funcionalidad
- `fix:` correccion de bug
- `docs:` cambios en documentacion
- `perf:` mejora de rendimiento
- `refactor:` refactorizacion
- `test:` agregar/modificar tests
- `chore:` tareas de mantenimiento

## Estandares
- Seguir REQUISITOS.md
- Comentar funciones publicas con Doxygen/JSDoc
- Mantener cobertura de tests > 80%
```

## 9. Empaquetado y distribucion

### 9.1 Estructura final del proyecto

```
logic_analizer_rpi/
├── README.md
├── CHANGELOG.md
├── CONTRIBUTING.md
├── LICENSE (MIT)
├── config.json
├── Makefile                # Build raiz (make, make test, make install)
│
├── server/
│   ├── README.md
│   ├── Makefile
│   ├── main.cpp            # Entry point
│   ├── gpio_mmap.h/.cpp    # GPIO lectura por mmap
│   ├── ring_buffer.h/.cpp  # SPSC lock-free buffer
│   ├── acquisition.h/.cpp  # Thread de adquisicion
│   ├── trigger.h/.cpp      # Deteccion de flancos
│   ├── measurements.h/.cpp # Mediciones (freq, duty, period)
│   ├── websocket.h/.cpp    # Servidor WebSocket
│   ├── http_server.h/.cpp  # Servidor HTTP + pagina web
│   ├── config.h/.cpp       # Configuracion
│   ├── logger.h/.cpp       # Logging
│   └── version.h           # Version从 config.json
│
├── web/
│   ├── README.md
│   ├── index.html          # Frontend completo (embebido en server)
│   └── assets/             # Iconos SVGs (opcional)
│       ├── icons/
│       │   ├── run.svg
│       │   ├── stop.svg
│       │   ├── single.svg
│       │   ├── rising.svg
│       │   ├── falling.svg
│       │   ├── zoom-in.svg
│       │   ├── zoom-out.svg
│       │   ├── export.svg
│       │   └── settings.svg
│       └── fonts/
│           └── (opcional, mejor usar system fonts)
│
├── scripts/
│   ├── install.sh          # Instalacion automatica
│   ├── install-service.sh  # systemd service
│   └── test.sh             # Tests automatizados
│
├── docs/
│   ├── README.md           # Indice de documentacion
│   ├── PROMPT.md           # Prompt completo para IA
│   ├── REQUISITOS.md       # Este archivo
│   ├── architecture.md     # Arquitectura
│   ├── setup.md            # Hardware setup
│   ├── protocol.md         # WebSocket protocolo
│   ├── firmware-pi.md      # Server firmware
│   ├── decoders.md         # Protocol decoders
│   ├── web-ui.md           # Frontend web
│   └── troubleshooting.md  # Problemas comunes
│
└── tests/
    ├── test_buffer.cpp     # Ring buffer tests
    ├── test_trigger.cpp    # Trigger detection tests
    └── test_decoders.js    # Decoder unit tests
```

### 9.2 systemd service

```ini
[Unit]
Description=Logic Analyzer Server
Documentation=https://github.com/siliconvalleyar-oss/logic_analizer_rpi
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=pi
Group=gpio
WorkingDirectory=/opt/logic-analyzer
ExecStart=/opt/logic-analyzer/server/logic_server --config /etc/logic-analyzer/config.json
Restart=on-failure
RestartSec=5
LimitNOFILE=65536
CPUAccounting=yes
MemoryAccounting=yes

[Install]
WantedBy=multi-user.target
```

### 9.3 Script de instalacion

```bash
#!/bin/bash
# install.sh - Instalacion completa del Analizador Logico

set -euo pipefail

VERSION="1.0.0"
INSTALL_DIR="/opt/logic-analyzer"
CONFIG_DIR="/etc/logic-analyzer"

echo "=== Logic Analyzer RPi v$VERSION - Installation ==="

# Verificar arquitectura
ARCH=$(uname -m)
case "$ARCH" in
    armv7l|armhf)   echo "Arch: 32-bit ARM" ;;
    aarch64|arm64)  echo "Arch: 64-bit ARM" ;;
    x86_64)         echo "Arch: x86_64 (simulation mode)" ;;
    *)              echo "Unsupported arch: $ARCH"; exit 1 ;;
esac

# Dependencias
echo "Installing dependencies..."
sudo apt update
sudo apt install -y build-essential cmake libssl-dev

# Compilar
echo "Building..."
cd server
make
cd ..

# Instalar binarios
echo "Installing to $INSTALL_DIR..."
sudo mkdir -p "$INSTALL_DIR/server" "$CONFIG_DIR"
sudo cp server/logic_server "$INSTALL_DIR/server/"
sudo cp config.json "$CONFIG_DIR/"

# Instalar systemd service
echo "Installing systemd service..."
sudo cp scripts/logic-analyzer.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable logic-analyzer
sudo systemctl start logic-analyzer

echo "=== Installation complete ==="
echo "Open http://$(hostname):8080 in your browser"
```

## 10. Metricas de calidad

| Aspecto | Indicador | Objetivo |
|---------|-----------|----------|
| Comentarios | % de funciones documentadas | 100% |
| Comentarios | Lineas de comentario / codigo | > 20% |
| Complejidad | Complejidad ciclomatica por funcion | < 10 |
| Testing | Cobertura de lineas | > 80% |
| Errores | Errores conocidos al release | 0 |
| Performance | Sps real vs especificado | > 90% |
| Memory | Memory leak en 24h | 0 bytes |
| UX | Tiempo de aprendizaje | < 5 min |
| UX | Clicks para ver forma de onda | < 3 |
| Codigo | Warnings de compilacion | 0 |
| Codigo | Duplicacion | < 5% |
