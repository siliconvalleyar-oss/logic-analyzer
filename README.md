# Analizador Logico Digital con Raspberry Pi

Analizador logico de **8 a 26 canales digitales** que usa los GPIO de una **Raspberry Pi** (2W, 3, 4, 5, Zero 2W — 32 o 64 bits) para capturar senales digitales en tiempo real y visualizarlas desde **cualquier navegador web** via WebSocket.

```
[Senales Digitales D0-D7] → [GPIO Raspberry Pi] → [WebSocket Server :8091]
                                                         ↓
                                              [HTTP Server :8080]
                                                         ↓
                                              [Navegador Web (Canvas)]
                                              [Celular / Tablet / PC]
```

## Repositorio

```
logic-analyzer/
├── README.md              # Este archivo
├── PROMPT.md              # Prompt completo para IA generar el proyecto
├── server/
│   ├── main.cpp           # Servidor C++ (mmap GPIO, WebSocket, HTTP)
│   └── Makefile           # Build system
├── web/
│   └── index.html         # Frontend web (Canvas + decodificadores)
└── docs/
    ├── ARCHITECTURE.md    # Arquitectura del sistema
    ├── SETUP.md           # Hardware: pines, conexion, instalacion
    ├── PROTOCOL.md        # Protocolo WebSocket JSON
    ├── FIRMWARE-PI.md     # Server Python y C++
    ├── DECODERS.md        # I2C, UART, SPI, PWM decodificadores
    └── TROUBLESHOOTING.md # Problemas comunes y soluciones
```

## Arranque rapido

```bash
# 1. En la Raspberry Pi, instalar dependencias
sudo apt install python3 python3-pip gpiod
pip install websockets

# 2. Version Python minima (server_logic.py del PROMPT)
python3 server_logic.py

# 3. O version C++ nativa
cd server && make && ./logic_server 8080

# 4. Abrir desde CUALQUIER dispositivo en la red:
#    http://raspberrypi:8080
```

## Stack de desarrollo

| Version | Lenguaje | Sps | Canales | CPU | Estado |
|---------|----------|-----|---------|-----|--------|
| 1 - Rapida | Python + gpiod polling | ~200 kSps | 8 | 100% 1 core | Listo |
| 2 - Eficiente | Python + gpiod edge events | ~500 kSps | 16 | ~30% 1 core | Listo |
| 3 - Nativa | C++ + mmap /dev/gpiomem | ~5 MSps | 26 | ~20% 1 core | En servidor/ |
| 4 - Pico W | C PIO (SDK) | 10 MSps | 16 | 0% CPU | Futuro |

## Protocolos decodificables (en JS, en el navegador)

- **I2C** — START/STOP, direccion 7-bit, R/W, ACK/NACK
- **UART** — baud configurable, bits, parity, stop
- **SPI** — CPOL/CPHA, MOSI+MISO, CS
- **PWM** — frecuencia, duty cycle, ton, toff

## Caracteristicas

- Trigger por flanco (rising, falling, either) en cualquier canal
- Cursors A/B con delta tiempo y frecuencia
- Zoom horizontal (rueda) y vertical (Ctrl+rueda)
- Timebase de 1us/div a 100ms/div
- Export CSV y screenshot PNG
- Modos RUN, STOP, SINGLE
- Pagina web auto-contenida (1 solo HTML, sin dependencias externas)
- Funciona en cualquier navegador: Chrome, Firefox, Safari, Edge
- Responsive: 320px (celular) a 4K (monitor)

## Especificaciones

| Parametro | Python polling | C++ mmap |
|-----------|---------------|----------|
| Max sample rate | ~200 kSps | ~5 MSps |
| Canales simultaneos | 8-16 | 26 |
| Buffer | 4096 muestras | 4096-65536 |
| Resolucion temporal | 5 us | 0.2 us |
| Protocolo | WebSocket JSON | WebSocket JSON |
| Voltaje | 3.3V (TTL) | 3.3V (TTL) |
| Trigger | software | software |
