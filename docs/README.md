# Documentacion del Proyecto

## Indice

| Archivo | Contenido |
|---------|-----------|
| `PROMPT.md` | Prompt completo para que una IA genere TODO el proyecto |
| `REQUISITOS.md` | Estandares profesionales: codigo, arquitectura, UI/UX, performance, testing, seguridad |
| `TODO.md` | Roadmap y tareas pendientes por version |
| `architecture.md` | Arquitectura del sistema: sampling, buffer, formatos, comparativa por modelo Pi |
| `setup.md` | Hardware: pin mappings por modelo (Pi 2W, 4, 5), conexion, divisor 5V, instalacion |
| `protocol.md` | Protocolo WebSocket JSON: comandos, formato bitfield, triggers, capabilities |
| `firmware-pi.md` | Server: codigo Python listo, edge events, C++ mmap, tabla rendimiento, systemd |
| `decoders.md` | Decodificadores: I2C, UART, SPI, PWM — logica, configuracion, renderizado |
| `troubleshooting.md` | Problemas comunes: conexion, GPIO, WebSocket, rendimiento |

## Para empezar

1. `../README.md` — Vista general del proyecto y arranque rapido
2. `setup.md` — Conectar los pines a la Raspberry Pi
3. `firmware-pi.md` — Compilar y ejecutar el servidor
4. `protocol.md` — Entender el protocolo WebSocket
5. `decoders.md` — Decodificar protocolos I2C/UART/SPI
6. `troubleshooting.md` — Si algo no funciona

## Para desarrolladores

1. `REQUISITOS.md` — Leer los estandares primero
2. `PROMPT.md` — Prompt para regenerar con IA
3. `TODO.md` — Proximas tareas y bugs conocidos
