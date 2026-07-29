# Analizador Logico — Arquitectura

## Vision General
Analizador logico de **8+ canales digitales** que lee pines GPIO en tiempo real desde una Raspberry Pi (cualquier modelo con GPIO) y muestra el estado logico (0/1) en una pagina web. Los datos viajan por **WebSocket** y se renderizan en **Canvas nativo**.

```
[Senales Digitales] → [GPIO Pi] → [WebSocket] → [Navegador Web → Canvas]
```

## Sampling Methods por modelo

| Metodo | Max Samples/s | Canales | CPU | Modelos compatibles |
|--------|--------------|---------|-----|-------------------|
| Polling Python | ~200 kSps | 8-16 | 100% un core | Pi 2W, 3, 4, 5 |
| Edge events (libgpiod) | ~1 MSps | 8-26 | ~30% un core | Pi 2W, 3, 4, 5 |
| C++ mmap GPIO | ~5 MSps | 8-26 | ~20% un core | Pi 2W, 3, 4, 5 |
| PIO (Pico W) | 10 MSps | 16 | 0% CPU | Pico W |

## Arquitectura de software

```
┌──────────────────────────────┐
│       Navegador Web          │
│  ┌────────────────────────┐  │
│  │ HTML+CSS+JS (1 archivo)│  │
│  │ Canvas API              │  │
│  │ WebSocket cliente       │  │
│  │ Decodificadores JS      │  │
│  │ LTTB / Cursors / Zoom   │  │
│  └──────────┬─────────────┘  │
└─────────────┼────────────────┘
              │ WebSocket JSON :8091
┌─────────────┼────────────────┐
│  Raspberry Pi 2W/4/5        │
│  ┌──────────┴─────────────┐  │
│  │ Server Python (asyncio) │  │
│  │ ── gpiod polling/events │  │
│  │ ── buffer circular      │  │
│  │ ── trigger detection    │  │
│  │ ── broadcast clientes   │  │
│  └────────────────────────┘  │
│  ┌────────────────────────┐  │
│  │ HTTP Server :8080      │  │
│  │ Sirve HTML + REST /api │  │
│  └────────────────────────┘  │
└─────────────┬────────────────┘
              │ GPIO
┌─────────────┴────────────────┐
│  Circuito bajo prueba (DUT)  │
│  Pines: CLK, DATA, CS, etc.  │
└──────────────────────────────┘
```

## Formatos de datos WebSocket

### Formato completo (legible)
```json
{
  "type":"waveform",
  "pins":[17,22,23,24],
  "data":[
    {"t":0,"states":[0,1,0,1]},
    {"t":10,"states":[0,1,1,0]}
  ],
  "t0":1234567,
  "dt":1,
  "labels":{"17":"CLK","22":"DATA"}
}
```

### Formato optimizado (bitset, recomendado para >4 canales)
```json
{
  "type":"waveform",
  "pins":[17,22,23,24],
  "timestamps":[0,10,20,30],
  "states":[0b0101, 0b0110, 0b0111, 0b0010],
  "t0":1234567,
  "rate":1000000
}
```
Cada `states[i]` es un entero donde el bit j corresponde al pin `pins[j]`.

## Consideraciones 32 vs 64 bits

| Aspecto | 32-bit (Pi 2W, Pi Zero 2W) | 64-bit (Pi 3+, Pi 4, Pi 5) |
|---------|---------------------------|---------------------------|
| SO recomendado | Raspberry Pi OS Lite 32 | Raspberry Pi OS Lite 64 |
| Python | armv7l, max 3GB RAM | aarch64, RAM completa |
| gpiod | Funciona igual | Funciona igual |
| max samples Python | ~150 kSps | ~200 kSps (mejor pipeline) |
| max samples C++ | ~3 MSps | ~5 MSps (SIMD, mejor cache) |
| Big integers JS | normal | normal |
| WebAssembly | no | si (64-bit) |

## Buffer circular
- Tamaño fijo: 4096 eventos (cambio de estado) o 65536 muestras (polling)
- SPSC (single producer = thread GPIO, single consumer = thread WebSocket)
- La web pide la cantidad de muestras que necesita segun timebase
- Si buffer se llena, descarta las mas viejas (modo roll infinito)
