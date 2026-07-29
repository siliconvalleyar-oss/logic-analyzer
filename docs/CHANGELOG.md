# Changelog

## [1.0.1] - 2026-07-28

### Added
- `docs/REQUISITOS.md`: Estandares profesionales de codigo, arquitectura, UI/UX, performance, testing, seguridad
- `docs/`: Documentacion completa del proyecto

### Fixed
- `server/.gitignore`: Binarios compilados excluidos del repo
- `reference_www/`: Eliminado del repositorio (terceros, solo referencia local)

---

## [1.0.0] - 2026-07-28

### Added
- Servidor C++ con mmap GPIO (~5 MSps, 26 canales)
- Servidor WebSocket nativo (RFC 6455, sin dependencias)
- Servidor HTTP embebido con pagina web integrada
- SHA-1 + Base64 para handshake WebSocket (implementacion propia)
- Buffer circular lock-free SPSC para adquisicion
- Trigger por flanco: rising, falling, both, high, low
- Modo simulacion con patrones de prueba multi-protocolo
- Makefile con deteccion automatica de arquitectura (ARM/ARM64/x86)
- Pagina web con Canvas rendering de formas digitales
- Cursors A/B interactivos con medicion de delta tiempo
- Zoom horizontal (rueda) y vertical (Ctrl+rueda)
- Timebase ajustable: 1us/div a 100ms/div
- Modos RUN, STOP, SINGLE
- Export CSV y screenshot PNG
- Diseno responsivo (320px-4K)
- Tema oscuro profesional
- Documentacion completa del protocolo WebSocket
- Prompt para IA (`PROMPT.md`) para regenerar el proyecto
- Guia de configuracion por modelo de Raspberry Pi
- Referencia de decodificadores (I2C, UART, SPI, PWM)

### Architecture
- `server/main.cpp` (772 lines) — Servidor completo en un modulo
- `web/index.html` (1152 lines) — Frontend completo en un archivo
- `docs/` — Documentacion tecnica y prompts
