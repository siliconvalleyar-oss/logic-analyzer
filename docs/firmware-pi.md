# Analizador Logico — Firmware para Raspberry Pi

## Opciones de implementacion

| Metodo | Lenguaje | Max Sps | CPU | RAM | Complejidad |
|--------|----------|---------|-----|-----|-------------|
| Polling simple | Python | ~200 kSps | 100% 1 core | Baja | Baja |
| Edge events | Python + libgpiod | ~500 kSps | ~30% 1 core | Baja | Media |
| Threaded polling | C++ | ~3 MSps | 100% 1 core | Media | Media |
| mmap /dev/gpiomem | C++ | ~5 MSps | ~20% 1 core | Media | Alta |
| DMA + PIO (Pico W) | C (SDK) | 10 MSps | 0% CPU | Baja | Alta |

## Opcion 1: Python + polling (recomendado para empezar)

### server_logic.py
```python
import asyncio
import websockets
import json
import gpiod
import time

# Configuracion
PINS = [17, 22, 23, 24, 27]
RATE_HZ = 200000  # 200 kSps
BUFFER_SIZE = 4096

class LogicAnalyzer:
    def __init__(self):
        self.chip = gpiod.Chip('gpiochip0')
        self.lines = self.chip.get_lines(PINS)
        self.lines.request(consumer='logic_analyzer', type=gpiod.LINE_REQ_DIR_IN)
        self.running = False
        self.buffer = []  # [(timestamp_ns, states_bitfield), ...]
        self.clients = set()

    async def acquire(self):
        """Loop de adquisicion por polling"""
        period_ns = 1_000_000_000 // RATE_HZ
        while self.running:
            t0 = time.monotonic_ns()
            values = self.lines.get_values()
            bitfield = sum(v << i for i, v in enumerate(values))
            self.buffer.append((t0, bitfield))
            if len(self.buffer) > BUFFER_SIZE:
                self.buffer.pop(0)
            # Sincronizar a tasa deseada
            elapsed = time.monotonic_ns() - t0
            sleep_ns = period_ns - elapsed
            if sleep_ns > 0:
                await asyncio.sleep(sleep_ns / 1e9)

    async def broadcast(self, msg):
        if self.clients:
            await asyncio.gather(*[c.send(json.dumps(msg)) for c in self.clients], return_exceptions=True)

    async def send_waveform(self):
        """Enviar buffer cada ~50ms"""
        while self.running:
            if self.buffer and self.clients:
                # Enviar ultimas 1024 muestras
                samples = self.buffer[-1024:]
                msg = {
                    "type": "waveform",
                    "pins": PINS,
                    "timestamps": [s[0] for s in samples],
                    "states": [s[1] for s in samples],
                    "t0": samples[0][0],
                    "rate": RATE_HZ
                }
                await self.broadcast(msg)
            await asyncio.sleep(0.05)

    async def handle(self, websocket):
        self.clients.add(websocket)
        try:
            async for raw in websocket:
                cmd = json.loads(raw)
                if cmd["cmd"] == "run":
                    self.running = True
                    asyncio.create_task(self.acquire())
                    asyncio.create_task(self.send_waveform())
                elif cmd["cmd"] == "stop":
                    self.running = False
                elif cmd["cmd"] == "set_pins":
                    # Reconfigurar pines (requiere parar)
                    pass
        finally:
            self.clients.discard(websocket)

async def main():
    analyzer = LogicAnalyzer()
    async with websockets.serve(analyzer.handle, "0.0.0.0", 8091):
        await asyncio.Future()

asyncio.run(main())
```

### Rendimiento por modelo con este codigo

| Modelo | OS | Python | Max Sps practico |
|--------|----|--------|-----------------|
| Pi Zero 2W | 32-bit | 3.11 | ~120 kSps |
| Pi 2W | 32-bit | 3.11 | ~150 kSps |
| Pi 3B+ | 64-bit | 3.11 | ~180 kSps |
| Pi 4 (2GB) | 64-bit | 3.11 | ~200 kSps |
| Pi 5 | 64-bit | 3.11 | ~220 kSps |

## Opcion 2: Edge events con libgpiod (menos CPU, mas eficiente)

```python
import gpiod

# Configurar monitoreo por eventos
line = chip.get_line(17)
line.request(consumer='trigger', type=gpiod.LINE_REQ_EV_BOTH_EDGES)

while True:
    event = line.event_wait(sec=1)
    if event:
        ev = line.event_read()
        # ev.event_type: 0=rising, 1=falling
        # ev.timestamp: nanosegundos
        print(f"GPIO17 {'RISING' if ev.event_type == 0 else 'FALLING'} at {ev.timestamp}")
```

**Ventaja**: CPU ~30% en vez de 100%. Desventaja: solo captura transiciones, no estados continuos.

## Opcion 3: C++ con mmap (maximo rendimiento en Pi 4/5)

### logic_server.cpp (fragmento)
```cpp
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

// GPIO registers (BCM2711 para Pi 4, BCM2712 para Pi 5)
#define GPIO_BASE   0xFE200000  // Pi 4
// #define GPIO_BASE 0x1F00000000 // Pi 5 (RP1)

volatile uint32_t *gpio;

void setup_gpio() {
    int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
    gpio = (uint32_t *)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_BASE);
}

uint32_t read_all_pins() {
    return gpio[13] & 0x0FFFFFFF;  // GPLEV0 register
}
```

### Compilacion para 32 vs 64 bits
```bash
# 32-bit
g++ -O3 -march=armv8-a -mtune=cortex-a72 logic_server.cpp -o logic_server

# 64-bit (Pi 4/5)
g++ -O3 -march=armv8-a+crc+simd -mtune=cortex-a72 logic_server.cpp -o logic_server

# 64-bit (Pi 2W / Zero 2W)
g++ -O3 -march=armv8-a -mtune=cortex-a53 logic_server.cpp -o logic_server
```

## Service systemd

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

## Verificacion de arquitectura
```python
import platform
print(f"Machine: {platform.machine()}")       # armv7l o aarch64
print(f"Processor: {platform.processor()}")
print(f"Python: {platform.python_version()}")

# Verificar gpiod disponible
import gpiod
chip = gpiod.Chip('gpiochip0')
print(f"Chip: {chip.name}, lines: {chip.num_lines}")
```
