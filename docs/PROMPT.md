# PROMPT: Analizador Logico Digital en Tiempo Real con Raspberry Pi

## Objetivo
Generar un analizador logico de **8+ canales digitales** que lea pines GPIO de una **Raspberry Pi** (cualquier modelo 2W/3/4/5, 32 o 64 bits) y muestre el estado logico de cada pin en una **pagina web** accesible desde CUALQUIER dispositivo de la red (celular, tablet, PC). Los datos viajan por **WebSocket** en tiempo real.

```
[Senales Digitales D0-D7] → [GPIO Raspberry Pi] → [WebSocket Server :8091]
                                                         ↓
                                              [HTTP Server :8080]
                                                         ↓
                                              [Pagina Web HTML+Canvas]
                                              [Accesible desde cualquier navegador]
```

## Hardware Soportado

| Modelo | CPU | RAM | GPIOs | Max Sps (Python) | Max Sps (C++) | Arch |
|--------|-----|-----|-------|-----------------|---------------|------|
| Pi Zero 2W | Cortex-A53 1GHz | 512MB | 26 | ~120 kSps | ~2 MSps | armv7l |
| Pi 2W | Cortex-A53 1.2GHz | 512MB | 26 | ~150 kSps | ~3 MSps | armv7l |
| Pi 3B+ | Cortex-A53 1.4GHz | 1GB | 26 | ~180 kSps | ~4 MSps | aarch64 |
| Pi 4 | Cortex-A72 1.8GHz | 1-8GB | 26 | ~220 kSps | ~5 MSps | aarch64 |
| Pi 5 | Cortex-A76 2.4GHz | 4-8GB | 26 | ~250 kSps | ~6 MSps | aarch64 |

## Conexion de pines

```
DUT (circuito a analizar)          Raspberry Pi GPIO
┌──────────────────┐              ┌─────────────────┐
│ Senal 1 (CLK)   ─┼──────────────┤ GPIO17 (pin 11) │  D0
│ Senal 2 (DATA)  ─┼──────────────┤ GPIO22 (pin 15) │  D1
│ Senal 3 (CS)    ─┼──────────────┤ GPIO23 (pin 16) │  D2
│ Senal 4 (MOSI)  ─┼──────────────┤ GPIO24 (pin 18) │  D3
│ Senal 5 (MISO)  ─┼──────────────┤ GPIO27 (pin 13) │  D4
│ Senal 6 (TX)    ─┼──────────────┤ GPIO4  (pin 7)  │  D5
│ Senal 7 (RX)    ─┼──────────────┤ GPIO5  (pin 29) │  D6
│ Senal 8 (INT)   ─┼──────────────┤ GPIO6  (pin 31) │  D7
│ GND             ─┼──────────────┤ GND              │
└──────────────────┘              └─────────────────┘
```

**ATENCION**: Todos los GPIO de Raspberry Pi son **3.3V**. Para senales de 5V usar divisor resistivo: `[DUT 5V]──[2.2kΩ]──┬──[GPIO]──[1kΩ]──[GND]`.

## Arquitectura de Software

```
┌──────────────────────────────────────────────────┐
│                Raspberry Pi                       │
│                                                   │
│  ┌──────────────────────────────────────────┐     │
│  │  server_logic.py (Python asyncio)        │     │
│  │                                          │     │
│  │  ┌──────────┐   ┌───────────────────┐   │     │
│  │  │ GPIO Poll │──>│ Circular Buffer   │   │     │
│  │  │ (gpiod)   │   │ (4096 muestras)   │   │     │
│  │  └──────────┘   └────────┬──────────┘   │     │
│  │                           │              │     │
│  │  ┌────────────────────────┴──────────┐   │     │
│  │  │ WebSocket Server :8091            │   │     │
│  │  │ Broadcast a todos los clientes    │   │     │
│  │  └───────────────────────────────────┘   │     │
│  │                                          │     │
│  │  ┌───────────────────────────────────┐   │     │
│  │  │ HTTP Server :8080                 │   │     │
│  │  │ Sirve index.html + API REST       │   │     │
│  │  └───────────────────────────────────┘   │     │
│  └──────────────────────────────────────────┘     │
└──────────────────────┬───────────────────────────┘
                       │ Red local (Wi-Fi o Ethernet)
┌──────────────────────┴───────────────────────────┐
│           Navegador Web (cualquier dispositivo)   │
│                                                   │
│  index.html (unico archivo, todo incluido)        │
│  ┌──────────────┐  ┌─────────────────────────┐    │
│  │ CSS tema      │  │ Canvas API              │    │
│  │ oscuro        │  │ Formas de onda digitales│    │
│  └──────────────┘  └──────────┬──────────────┘    │
│                               │                    │
│  ┌────────────────────────────┴──────────────┐     │
│  │ JavaScript:                               │     │
│  │  - WebSocket client → ws://:8091          │     │
│  │  - LTTB downsampling                      │     │
│  │  - Trigger detection                      │     │
│  │  - Protocol decoders (I2C, UART, SPI...)  │     │
│  │  - Cursors A/B + zoom + pan              │     │
│  │  - Export CSV / Screenshot               │     │
│  └─────────────────────────────────────────────┘   │
└───────────────────────────────────────────────────┘
```

## Server Python (server_logic.py)

### Requisitos
- Python 3.9+
- `gpiod` (lectura GPIO)
- `websockets` (>=10.0)
- `aiohttp` o http.server nativo (servir HTML)

### Instalacion
```bash
sudo apt install python3 python3-pip gpiod
pip install websockets
```

### Logica de adquisicion

```python
import asyncio
import websockets
import json
import gpiod
import struct
import time

PINS = [17, 22, 23, 24, 27, 4, 5, 6]
RATE_HZ = 200000   # 200 kSps en Python
BUFFER_SIZE = 4096
WS_PORT = 8091
HTTP_PORT = 8080

class LogicAnalyzer:
    def __init__(self):
        self.chip = gpiod.Chip('gpiochip0')
        self.lines = self.chip.get_lines(PINS)
        self.lines.request(consumer='logic', type=gpiod.LINE_REQ_DIR_IN)
        self.clients = set()
        self.running = False
        self.buffer = []  # [(timestamp_ns, bitfield), ...]
        self.trigger_config = {"pin": None, "type": None, "hpos": 50}
        self.mode = "stop"  # run | stop | single | armed

    async def acquire_loop(self):
        """Polling a tasa fija"""
        period_ns = 1_000_000_000 // RATE_HZ
        while self.running:
            t0 = time.monotonic_ns()
            values = self.lines.get_values()
            # Empaquetar 8 pines en un byte (bit 0 = pin[0], bit 7 = pin[7])
            bitfield = 0
            for i, v in enumerate(values):
                bitfield |= (v << i)
            self.buffer.append((t0, bitfield))
            if len(self.buffer) > BUFFER_SIZE:
                self.buffer.pop(0)
            # Sincronizar a la tasa deseada
            elapsed = time.monotonic_ns() - t0
            sleep_ns = period_ns - elapsed
            if sleep_ns > 0:
                await asyncio.sleep(sleep_ns / 1e9)

    def check_trigger(self, prev, curr):
        """Detectar flanco. prev/curr son bitfields"""
        if self.trigger_config["pin"] is None:
            return False
        pin_idx = PINS.index(self.trigger_config["pin"])
        prev_bit = (prev >> pin_idx) & 1
        curr_bit = (curr >> pin_idx) & 1
        t = self.trigger_config["type"]
        if t == "rising" and prev_bit == 0 and curr_bit == 1:
            return True
        if t == "falling" and prev_bit == 1 and curr_bit == 0:
            return True
        if t == "either" and prev_bit != curr_bit:
            return True
        return False

    async def broadcast(self, msg):
        if not self.clients:
            return
        data = json.dumps(msg)
        await asyncio.gather(*[c.send(data) for c in self.clients], return_exceptions=True)

    async def send_waveform_loop(self):
        """Enviar buffer cada ~50ms (20 fps)"""
        while self.running or self.mode == "single":
            if len(self.buffer) < 2:
                await asyncio.sleep(0.01)
                continue
            # Tomar las ultimas N muestras
            count = min(1024, len(self.buffer))
            samples = self.buffer[-count:]
            msg = {
                "type": "waveform",
                "pins": PINS,
                "timestamps": [s[0] for s in samples],
                "states": [s[1] for s in samples],
                "t0": samples[0][0],
                "dt_us": 1_000_000 // RATE_HZ,
                "rate": RATE_HZ,
                "trigger_index": -1
            }
            await self.broadcast(msg)
            # En SINGLE, enviar una vez y parar
            if self.mode == "single":
                self.running = False
                self.mode = "stop"
                break
            await asyncio.sleep(0.05)

    async def handle_client(self, websocket):
        self.clients.add(websocket)
        try:
            # Enviar estado inicial
            await websocket.send(json.dumps({
                "type": "state",
                "mode": self.mode,
                "rate": RATE_HZ,
                "pins": PINS,
                "clients": len(self.clients)
            }))
            async for raw in websocket:
                cmd = json.loads(raw)
                action = cmd.get("cmd")
                if action == "run":
                    self.mode = "run"
                    self.running = True
                    self.buffer.clear()
                    asyncio.create_task(self.acquire_loop())
                    asyncio.create_task(self.send_waveform_loop())
                elif action == "stop":
                    self.running = False
                    self.mode = "stop"
                elif action == "single":
                    self.mode = "armed"
                    self.running = True
                    self.buffer.clear()
                    asyncio.create_task(self.acquire_loop())
                elif action == "arm":
                    self.mode = "armed"
                elif action == "set_pins":
                    # Reconfigurar pines (logica simplificada)
                    pass
                elif action == "set_trigger":
                    self.trigger_config["pin"] = cmd.get("pin")
                    self.trigger_config["type"] = cmd.get("type")
                    self.trigger_config["hpos"] = cmd.get("hpos", 50)
                elif action == "set_rate":
                    # Cambiar tasa (simplificado)
                    pass
                elif action == "get_state":
                    await websocket.send(json.dumps({
                        "type": "state",
                        "mode": self.mode,
                        "rate": RATE_HZ,
                        "pins": PINS,
                        "samples": len(self.buffer),
                        "clients": len(self.clients)
                    }))
                elif action == "get_capabilities":
                    await websocket.send(json.dumps({
                        "type": "capabilities",
                        "max_pins": len(PINS),
                        "max_rate_hz": 1000000,
                        "modes": ["polling", "edge"],
                        "protocols": ["i2c", "uart", "spi", "pwm"],
                        "model": "Raspberry Pi",
                        "arch": "aarch64"
                    }))
        except websockets.exceptions.ConnectionClosed:
            pass
        finally:
            self.clients.discard(websocket)

async def http_handler(request):
    """Servir pagina web estatica"""
    return web.Response(text=HTML_PAGE, content_type='text/html')

async def main():
    analyzer = LogicAnalyzer()
    # Servidor WebSocket
    ws_server = await websockets.serve(analyzer.handle_client, "0.0.0.0", WS_PORT)
    # Servidor HTTP para la pagina web
    app = web.Application()
    app.router.add_get('/', lambda r: web.Response(text=HTML_PAGE, content_type='text/html'))
    app.router.add_get('/{tail:.*}', lambda r: web.Response(text=HTML_PAGE, content_type='text/html'))
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', HTTP_PORT)
    await site.start()
    print(f"Logic Analyzer running:")
    print(f"  WebSocket: ws://0.0.0.0:{WS_PORT}")
    print(f"  Web page:  http://0.0.0.0:{HTTP_PORT}")
    await asyncio.Future()  # correr para siempre

# Si no usas aiohttp, podes usar http.server:
# from http.server import HTTPServer, BaseHTTPRequestHandler
# class Handler(BaseHTTPRequestHandler):
#     def do_GET(self):
#         self.send_response(200)
#         self.send_header('Content-Type', 'text/html')
#         self.end_headers()
#         self.wfile.write(HTML_PAGE.encode())
```

## Pagina Web (index.html) — TODO EN UN ARCHIVO

### Requisitos de la pagina
- **Un solo archivo HTML** con CSS embebido y JavaScript inline
- Sin dependencias externas, sin CDN, sin frameworks
- Funciona offline si se guarda localmente
- Responsive: 320px minimo (celular) hasta 4K (monitor)
- Tema oscuro tipo osciloscopio

### UI Layout

```
┌─────────────────────────────────────────────────────────┐
│ [host:raspberrypi:8091] [Connect] ● CONNECTED  200 kHz  │
├─────────────────────────────────────────────────────────┤
│ [+] Add Pin   [RUN ▼] [STOP] [SINGLE]  [10us/div ▼]   │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  D0 CLK  ━━━┯━━━━━━━┯━┯━┯━━━━━━━━━┯━━━━━━━             │
│              │       │ │ │         │                     │
│  D1 DATA  ━━━┷━━━━━━━┷━┷━┷━━━━━━━━━┷━━━━━━━             │
│                                                          │
│  D2 CS    ━━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━┯━              │
│              │ │ │ │ │ │ │ │ │ │ │ │ │ │                 │
│  D3 MOSI  ━━━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━┷━              │
│                                                          │
│  D4 MISO  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━                  │
│                                                          │
│  D5 TX    ━━━━┯━━━━━┯━━━━━┯━━━━━┯━━━━━                  │
│                │     │     │     │                        │
│  D6 RX    ━━━━━┷━━━━━┷━━━━━┷━━━━━┷━━━━━                  │
│                                                          │
│  D7 INT   ━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━━                │
│                       │                                  │
│  ▲ Trigger at 50%    Muestras: 1024    Cursor: t=532us  │
├─────────────────────────────────────────────────────────┤
│ [Decode ▼] [I2C ▼] [Export CSV] [Zoom+] [Zoom-] [Reset]│
├─────────────────────────────────────────────────────────┤
│ Decoded: START 0x48 W ACK DATA:0xAB ACK DATA:0xCD ACK   │
│          STOP   I2C transaction 3 bytes                  │
└─────────────────────────────────────────────────────────┘
```

### Canvas rendering
- Fondo oscuro `#1a1a2e`
- Cada canal es una fila horizontal con su nombre a la izquierda
- Color por canal: D0=#FF6B6B, D1=#4ECDC4, D2=#45B7D1, D3=#96CEB4, D4=#FFEAA7, D5=#DDA0DD, D6=#98D8C8, D7=#F7DC6F
- Estado HIGH = parte superior de la fila, LOW = parte inferior
- Linea vertical divisoria cada 10 divisiones
- Trigger: triangulo rojo ▲ en el punto de disparo
- Cursor A (rojo) y Cursor B (azul) con Δt mostrado

### Controles
- **RUN**: adquiere continuamente, actualiza canvas 20 fps
- **STOP**: congela la ultima captura
- **SINGLE**: arma trigger, captura al disparar, se congela
- **Timebase**: 1us, 5us, 10us, 50us, 100us, 500us, 1ms, 5ms, 10ms, 50ms, 100ms/div
- **Zoom**: rueda del raton = zoom horizontal, Ctrl+rueda = zoom vertical (altura filas)
- **Pan**: click+arrastre para desplazar ventana de tiempo
- **Doble click**: reset zoom

### Trigger (en el navegador)
- Canal seleccionable entre los activos
- Tipos: rising edge, falling edge, either edge
- Posicion: slider 0-100% del buffer
- En modo SINGLE: boton "Arm", espera trigger, muestra captura y STOP

### Decodificadores de protocolo (en JavaScript, corren en el navegador)

**I2C**: SCL + SDA. Detecta START/STOP, direccion 7-bit, R/W, ACK/NACK, datos.
```javascript
function decodeI2C(timestamps, states, sclPin, sdaPin) {
    // timestamps: array de nanosegundos
    // states: array de enteros (bitfield)
    // Devuelve: [{t, type, label, color}, ...]
}
```

**UART**: 1 pin (RX o TX). Config: baud rate, bits, parity, stop bits.
```javascript
function decodeUART(timestamps, states, rxPin, baud) {
    // Devuelve: [{t, byte, char, parity_ok}, ...]
}
```

**SPI**: SCLK + MOSI + MISO + CS. Config: CPOL, CPHA, bits.
```javascript
function decodeSPI(timestamps, states, sclk, mosi, miso, cs, cpol, cpha) {
    // Devuelve: [{t, mosi_byte, miso_byte}, ...]
}
```

**PWM**: 1 pin. Mide ton, toff, frecuencia, duty cycle.
```javascript
function decodePWM(timestamps, states, pin) {
    // Devuelve: [{t, freq_hz, duty_pct, ton_us, toff_us}, ...]
}
```

Los items decodificados se dibujan como texto sobre las formas de onda, con colores: START=verde, direccion=azul, datos=naranja, STOP=rojo, ACK=circulo relleno, NACK=circulo hueco.

### Cursors interactivos
- **Cursor A** (linea roja): clic izquierdo en el canvas
- **Cursor B** (linea azul): Shift+clic izquierdo
- Muestra: tA, tB, Δt = tB - tA, 1/Δt (frecuencia)
- Los cursores se pueden arrastrar

### Mediciones en tiempo real
- **Frecuencia** del canal seleccionado (Hz)
- **Periodo** (s, ms, us)
- **Duty cycle** (%)
- **Ancho de pulso** positivo y negativo (us)
- **Cantidad de flancos** rising/falling por segundo
- **Tasa de bits** aproximada (baud)

### Exportacion
- **CSV**: descarga `logic_capture.csv` con columnas: time_ns, D0, D1, D2, ...
- **Screenshot**: descarga `logic_screenshot.png` del canvas
- **Copiar como binario**: seleccion entre cursores → portapapeles como "10101010..."

### Protocol WebSocket detallado

#### Comandos (cliente → servidor):
```json
{"cmd":"run"}
{"cmd":"stop"}
{"cmd":"single"}
{"cmd":"arm"}
{"cmd":"set_pins","pins":[17,22,23,24,27]}
{"cmd":"set_labels","labels":{"17":"CLK","22":"DATA"}}
{"cmd":"set_rate","hz":200000}
{"cmd":"set_trigger","pin":17,"type":"rising","hpos":50}
{"cmd":"set_timebase","value_us":10}
{"cmd":"set_mode","mode":"polling"}  // polling | edge
{"cmd":"get_state"}
{"cmd":"get_capabilities"}
```

#### Datos (servidor → cliente):
```json
{
  "type":"waveform",
  "pins":[17,22,23,24,27,4,5,6],
  "timestamps":[1234567000, 1234567005, 1234567010, ...],
  "states":[5, 13, 29, 61, 53, 37, ...],
  "t0":1234567000,
  "dt_us":5,
  "rate":200000,
  "trigger_index":512,
  "labels":{"17":"CLK","22":"DATA","23":"CS"}
}
```
Donde `states[i]` es un entero: bit 0 = pin[0], bit 1 = pin[1], etc.

#### Estado:
```json
{
  "type":"state",
  "mode":"run",
  "rate":200000,
  "pins":[17,22,23,24,27],
  "samples":4096,
  "clients":2,
  "buffer_usage":0.75
}
```

#### Decodificado:
```json
{
  "type":"decoded",
  "protocol":"i2c",
  "items":[
    {"t":100,"type":"start","label":"START","color":"#4CAF50"},
    {"t":140,"type":"address","label":"0x48 W","ack":true,"color":"#2196F3"},
    {"t":180,"type":"data","byte":0xAB,"ack":true,"color":"#FF9800"},
    {"t":220,"type":"stop","label":"STOP","color":"#F44336"}
  ]
}
```

#### Errores:
```json
{"type":"error","msg":"gpiod line request failed: Device or resource busy"}
```

#### Capabilities:
```json
{
  "type":"capabilities",
  "max_pins":26,
  "max_rate_hz":1000000,
  "modes":["polling","edge"],
  "protocols":["i2c","uart","spi","pwm"],
  "model":"Pi 4 Model B Rev 1.5",
  "arch":"aarch64",
  "kernel":"6.1.21-v8+"
}
```

## Version C++ de alto rendimiento (opcional)

Para maximizar sample rate en Pi 4/5, implementar el servidor en C++:

```cpp
// Leer GPIO por mmap a /dev/gpiomem
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define GPIO_BASE 0xFE200000  // Pi 4 (BCM2711)
#define BLOCK_SIZE 4096

volatile uint32_t *gpio_map;

void init_gpio() {
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    gpio_map = (uint32_t *)mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
    close(fd);
}

uint32_t read_all_pins() {
    // GPLEV0 register offset = 0x34 (13 words from base)
    return gpio_map[13] & 0x0FFFFFFF;  // bits 0-27 = GPIO0-27
}
```

Compilar con optimizaciones segun arquitectura:

```bash
# 32-bit (Pi 2W, Zero 2W)
g++ -O3 -march=armv8-a -mtune=cortex-a53 server.cpp -o server -lwebsockets -lpthread

# 64-bit (Pi 4)
g++ -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 server.cpp -o server -lwebsockets -lpthread

# 64-bit (Pi 5)
g++ -O3 -march=armv8.2-a+simd -mtune=cortex-a76 server.cpp -o server -lwebsockets -lpthread
```

Usar `libwebsockets` o `uWebSockets` para el servidor WebSocket en C++.

## Instalacion y despliegue

### 1. En la Raspberry Pi:
```bash
# Dependencias
sudo apt update && sudo apt install -y python3 python3-pip gpiod
pip install websockets

# Copiar server_logic.py
scp server_logic.py pi@raspberrypi:~

# Ejecutar
python3 server_logic.py
```

### 2. Acceder desde cualquier dispositivo:
```
Abrir navegador → http://raspberrypi:8080
```
Funciona en celular, tablet, PC, Smart TV... cualquier cosa con navegador web.

### 3. Auto-inicio con systemd:
```ini
[Unit]
Description=Logic Analyzer Server
After=network.target

[Service]
Type=simple
User=pi
ExecStart=/home/pi/logic-env/bin/python /home/pi/server_logic.py
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

## Estructura final del proyecto

```
logic_analyzer/
├── server_logic.py      # Servidor Python (WebSocket + HTTP + GPIO)
├── index.html           # Pagina web (todo en 1 archivo)
├── config.json          # Configuracion persistente
├── requirements.txt     # Dependencias Python
└── README.md            # Instrucciones

Opcional (alto rendimiento):
├── server.cpp           # Servidor C++ con mmap GPIO
├── CMakeLists.txt       # Build system
└── web/                 # Pagina web (servida como archivo estatico)
    └── index.html
```

## Resumen de entregables

| Archivo | Lenguaje | Lineas estimadas | Que hace |
|---------|----------|-----------------|----------|
| `server_logic.py` | Python | ~200 | Servidor WebSocket + HTTP + GPIO polling |
| `index.html` | HTML+CSS+JS | ~600 | Pagina web completa (canvas, decoders, cursors, export) |
| `config.json` | JSON | ~20 | Configuracion persistente |
| `requirements.txt` | Texto | ~3 | Dependencias |
| `README.md` | Markdown | ~50 | Instrucciones de instalacion y uso |

**IMPORTANTE**: La pagina web debe ser un solo archivo HTML autosuficiente. Sin CDN, sin frameworks, sin dependencias externas. Debe funcionar offline. Debe verse bien en celular (320px) y monitor 4K. Los decodificadores I2C, UART, SPI y PWM deben implementarse en JavaScript puro.
