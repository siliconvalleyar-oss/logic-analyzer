# TODO — Logic Analyzer RPi

> Documento maestro de features, mejoras, bugs y warnings.
> Cada sección lista tareas concretas agrupadas por área.

---

## v1.0 — Core funcional (entregado)

- [x] Servidor HTTP + WebSocket con epoll (C++17)
- [x] Lectura GPIO via mmap (RPi) o simulación (x86)
- [x] Buffer circular lock-free SPSC (65536 muestras)
- [x] Streaming de 26 canales a 500KHz-2MHz
- [x] Trigger por flanco: Rising, Falling, Both, High, Low
- [x] Pre-trigger configurable (Off/64/128/256/512/1K/2K/4K)
- [x] RUN/STOP/SINGLE con máquina de estados
- [x] Cursores A/B con medidas (frecuencia, duty, pulso, bin/hex/dec)
- [x] Decodificadores I2C, UART, SPI en JavaScript
- [x] Zoom/pan en canvas, auto-fit en primera carga
- [x] Persistencia de config en `config.json`

---

## v1.1 — Server-side decoders

- [ ] Decodificador I2C en C++ (enviar tramas decodificadas por WS)
- [ ] Decodificador UART en C++
- [ ] Decodificador SPI en C++
- [ ] Opción: enviar datos decodificados en vez de raw bitfield
- [ ] Opción: enviar raw + decodificado simultáneo

## v1.2 — Web UI avanzada

- [ ] **Timeline / overview**: barra inferior con vista completa + ventana de zoom arrastrable
- [ ] **Cursores A/B arrastrables** en el canvas (no solo por click)
- [ ] **Medidas automáticas**: frecuencia, periodo, duty cycle, ancho de pulso min/max por canal
- [ ] **Export CSV/VCD**: seleccionar rango con cursores y descargar
- [ ] **Export JSON**: captura completa descargable
- [ ] **Tema claro/oscuro** toggle
- [ ] **Feedback visual de conexión**: timeout, barra de progreso, botón cancelar

## v1.3 — Performance

- [ ] **Modo edge events**: usar libgpiod con eventos de flanco (en vez de polling). Reduce CPU ~90%
- [ ] **Compresión de datos**: enviar solo transiciones (diferencial) en vez de todas las muestras. Reduce ancho de banda x100+
- [ ] **Modo deep**: buffer de 1M+ muestras en RAM, enviando solo ventana visible
- [ ] **Zero-copy** en broadcast a clientes (evitar `std::string` temporal por batch)
- [ ] **Binary WebSocket** en vez de JSON para datos crudos (a 500KHz, JSON stringify toma ~30ms/batch)
- [ ] **Benchmark**: verificación de 0% pérdida en 1 hora a 2MHz

## v1.4 — Features avanzadas

- [ ] **Trigger por patrón**: ej: detectar secuencia "1010" en cualquier canal
- [ ] **Buscador de patrones**: post-captura, buscar cualquier secuencia en los datos
- [ ] **Máscara de canales**: ocultar/mostrar canales individuales desde la UI
- [ ] **Análisis de protocolo en tiempo real**: I2C/UART/SPI
- [ ] **Grabar captura**: guardar datos en archivo `.logic` o `.vcd`
- [ ] **Reproducir captura**: cargar archivo guardado y navegar como si fuera en vivo

## v1.5 — Multiplataforma

- [ ] Soporte para **Pico W con PIO** (10 MSps, 16 canales)
- [ ] **Cliente Qt** desktop (C++ nativo, sin navegador)
- [ ] **API REST** para control remoto (iniciar/parar/configurar)
- [ ] **Webhook** on trigger (POST a URL cuando el trigger dispara)

## v1.6 — Estabilidad y producción

- [ ] **Test harness** con Google Test:
  - `RingBuffer`: push/drain con wrap, vacío, lleno, concurrente SPSC
  - `trigger_find_index`: rising/falling/both/high/low con datos conocidos
  - `ws_decode/encode`: round-trip, payload >125/>65535, masked/unmasked
  - `config_parse_args`: flags, default, --config + override
  - `protocol`: build/parse round-trip
- [ ] **Test de resistencia**: 24h sin pérdida de datos a 2MHz
- [ ] **CI/CD**: GitHub Actions para compilar en ARM/ARM64/x64
- [ ] **Package .deb** para Raspberry Pi OS
- [ ] **Dashboard web** con historial de capturas

---

## Bugs

Ver [`BUGS.md`](BUGS.md).

> **Resumen**: 10 bugs identificados, 9 cerrados, 1 abierto (SINGLE + trigger sin condición de disparo).

---

## Warning fixes (compilación estricta)

Compilación remota en Raspberry Pi (aarch64, GCC 12) con `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wformat=2` genera **22 warnings, 0 bugs reales**.

| Archivo | Línea | Warning | Arreglo propuesto |
|---------|-------|---------|-------------------|
| `config.cpp` | 59,60,893,894 | `long int` → `size_t` (ftell) | `static_cast<size_t>(len)` |
| `config.cpp` | 92,106 | `&&` dentro de `\|\|` | Agregar paréntesis |
| `server.cpp` | 41 | `int` → `size_t` | `static_cast<size_t>()` + validar > 0 |
| `server.cpp` | 75 | `int` → `uint16_t` | Validar puerto 1-65535 |
| `server.cpp` | 135 | `int` → `uint64_t` | `static_cast<uint64_t>()` |
| `server.cpp` | 161 | chrono rep → `uint64_t` | `static_cast<uint64_t>()` |
| `server.cpp` | 317,499 | `size_t` → iter diff | Cast explícito |
| `server.cpp` | 477,821 | `ssize_t` → `size_t` | `static_cast<size_t>(n)` |
| `server.cpp` | 752 | `[fd,st]` shadows param | Renombrar `fd` → `cfd` |
| `ring_buffer.cpp` | 32,35,36 | `size_t` → iter diff | Cast explícito |
| `gpio.cpp` | 40 | `~(7 << bit)` signed | `~(uint32_t)(7u << bit)` |

**18 edits triviales** para limpiar todos los warnings y poder compilar con `-Werror`.

---

## Mejoras continuas (deuda técnica)

- [ ] Dividir `server/main.cpp` en módulos separados (gpio, ring_buffer, websocket, trigger, protocol, server, config, logger)
- [ ] `static_assert` para tamaños de struct (ej: `Sample` debe ser 12 bytes)
- [ ] Reemplazar `std::map<int, ClientState>` con `std::unordered_map`
- [ ] Memory pool para evitar allocaciones en polling loop
- [ ] Doxygen comments en todas las funciones públicas
- [ ] Unificar `pins_json()` como constante global
- [ ] Debounce/throttle en `config_save_file()` para no desgastar la SD
- [ ] Timeout en conexiones HTTP lentas (slow loris mitigation)
- [ ] Limitar `frame_buf` / `read_buf` por cliente (~16KB max)
- [ ] Modo "realistic simulation" en GPIO (con jitter y glitches)
- [ ] `clock_nanosleep` con TIMER_ABSTIME en polling loop (eliminar timing drift)
