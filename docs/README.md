# Documentacion del Proyecto

## Indice

| Archivo | Contenido |
|---------|-----------|
| `PROMPT.md` | Prompt completo para que una IA genere TODO el proyecto |
| `REQUISITOS.md` | Estandares profesionales: codigo, arquitectura, UI/UX, performance, testing, seguridad |
| `TODO.md` | Roadmap y tareas pendientes por version |
| `ARCHITECTURE.md` | Arquitectura del sistema: sampling, buffer, formatos, comparativa por modelo Pi |
| `SETUP.md` | Hardware: pin mappings por modelo (Pi 2W, 4, 5), conexion, divisor 5V, instalacion |
| `PROTOCOL.md` | Protocolo WebSocket JSON: comandos, formato bitfield, triggers, capabilities |
| `FIRMWARE-PI.md` | Server: codigo Python listo, edge events, C++ mmap, tabla rendimiento, systemd |
| `DECODERS.md` | Decodificadores: I2C, UART, SPI, PWM — logica, configuracion, renderizado |
| `TROUBLESHOOTING.md` | Problemas comunes: conexion, GPIO, WebSocket, rendimiento |

## Para empezar

1. `../README.md` — Vista general del proyecto y arranque rapido
2. `SETUP.md` — Conectar los pines a la Raspberry Pi
3. `FIRMWARE-PI.md` — Compilar y ejecutar el servidor
4. `PROTOCOL.md` — Entender el protocolo WebSocket
5. `DECODERS.md` — Decodificar protocolos I2C/UART/SPI
6. `TROUBLESHOOTING.md` — Si algo no funciona

## Para desarrolladores

1. `REQUISITOS.md` — Leer los estandares primero
2. `PROMPT.md` — Prompt para regenerar con IA
3. `TODO.md` — Proximas tareas y bugs conocidos
