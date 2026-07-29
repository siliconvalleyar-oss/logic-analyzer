# Changelog

## [1.6.0] - 2026-07-29

### Added
- **Pre-Trigger buffer**: Almacena N muestras antes del disparo para ver contexto previo a la condición de trigger
  - Selector en toolbar: Off / 64 / 128 / 256 / 512 / 1K / 2K / 4K
  - Zona pre-trigger sombreada en el waveform con línea delimitadora punteada
  - Control vía WebSocket: comando `set_pretrig` con profundidad en samples
  - Persistencia en `config.json` (campo `pre_trig_depth`)
- **Trigger completamente funcional**: Rising, Falling, High, Low, None
  - Máquina de estados con armado/desarmado en RUN/STOP/SINGLE
  - Detección por flanco en el polling loop (no post-procesamiento)
  - Timestamp exacto del punto de trigger para ubicación precisa en el waveform
  - Marcador "T" rojo con línea vertical sobre el punto de disparo
- **Decodificación de región seleccionada**: Análisis automático al posicionar cursores A/B
  - Representación binaria de la señal en la selección
  - Representación hexadecimal (agrupada de a 4 bits)
  - Representación decimal (hasta 53 bits)
  - Análisis de pulso: ancho HIGH, ancho LOW
  - Duty cycle porcentual
  - Período promedio y frecuencia
- **Auto-Fit en primera carga**: Ajuste automático de zoom al recibir datos por primera vez
  - Resetea al cambiar timebase
- **WebSocket auto-port**: Conexión WebSocket usando el mismo puerto desde donde se cargó la página
  - Soporte para `?connect=ws://host:port` URL parameter
- **Run/Stop cíclico sin bloqueos**: Liberación correcta de recursos en cada ciclo
  - Desarmado de trigger al pausar
  - Re-armado al reanudar
  - Reset pendiente para enviar buffer completo al reconectar

### Fixed
- **Trigger inactivo**: La lógica de trigger no se aplicaba (todos los modos producían el mismo resultado)
- **Run/Stop bloqueante**: La segunda ejecución de RUN después de STOP dejaba la aplicación congelada
- **Renderizado incompleto**: El waveform solo mostraba las primeras muestras del buffer
- **Puerto WebSocket hardcodeado**: Conexión fallaba cuando el servidor no estaba en puerto 8080

---

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
