# TODO — Logic Analyzer RPi

## v1.1.0 — Modularizacion del servidor C++

- [ ] Dividir `server/main.cpp` en modulos separados:
  - [ ] `server/gpio.h/.cpp` — Lectura GPIO (mmap + simulacion)
  - [ ] `server/ring_buffer.h/.cpp` — Buffer circular lock-free SPSC
  - [ ] `server/websocket.h/.cpp` — WebSocket frames + SHA1 + Base64
  - [ ] `server/trigger.h/.cpp` — Deteccion de flancos
  - [ ] `server/protocol.h/.cpp` — Mensajes JSON del protocolo
  - [ ] `server/server.h/.cpp` — Servidor HTTP + WebSocket (epoll)
  - [ ] `server/config.h/.cpp` — Configuracion desde JSON
  - [ ] `server/logger.h/.cpp` — Logging con archivo y rotacion
- [ ] Agregar Doxygen comments a todas las funciones publicas
- [ ] Hacer que `make test` compile y corra tests unitarios

## v1.2.0 — Decodificadores en C++ (server-side)

- [ ] Decodificador I2C en C++ (envia tramas decodificadas por WS)
- [ ] Decodificador UART en C++
- [ ] Decodificador SPI en C++
- [ ] Opcion para enviar datos decodificados en vez de raw bitfield
- [ ] Reducir ancho de banda en redes lentas

## v1.3.0 — Web UI mejorada

- [ ] Timeline / buffer overview (barra inferior con zoom)
- [ ] Arrastrar cursores A/B en el canvas
- [ ] Medidas automaticas: frecuencia, periodo, duty cycle
- [ ] Decodificadores I2C, UART, SPI en JavaScript
- [ ] Export CSV seleccionando rango con cursores
- [ ] Tema claro/oscuro toggle

## v1.4.0 — Performance

- [ ] Modo edge events con libgpiod (menos CPU)
- [ ] Compresion de datos (diferencial o run-length)
- [ ] Buffer de 65536 muestras en modo profundo
- [ ] Zero-copy en el broadcast a clientes
- [ ] Benchmark: verificacion de 0% perdida en 1 hora

## v1.5.0 — Features avanzadas

- [ ] Analizador de protocolo I2C en tiempo real
- [ ] Analizador de protocolo UART en tiempo real
- [ ] Analizador de protocolo SPI en tiempo real
- [ ] Buscador de patrones (ej: "1010" en cualquier canal)
- [ ] Trigger por patron en vez de solo flanco
- [ ] Mascara de canales (ocultar/mostrar)

## v1.6.0 — Trigger Activo, Pre-Trigger, Auto-Fit, Decodificación de Selección

- [x] **Trigger funcional**: Rising, Falling, High, Low, None con máquina de estados
- [x] **Pre-Trigger buffer**: muestras previas al disparo (configurable Off/64/128/256/512/1K/2K/4K)
- [x] **Run/Stop cíclico**: sin bloqueos ni resource leaks
- [x] **Decodificación de selección**: binario, hex, decimal, pulso, duty cycle con cursores A/B
- [x] **Auto-Fit en primera carga**: zoom automático al recibir datos
- [x] **Persistencia de pre-trigger**: guardado en config.json
- [x] **WebSocket dinámico**: usa el mismo puerto de la página cargada

## v1.7.0 — Multiplataforma

- [ ] Soporte para Pico W con PIO (10 MSps, 16 canales)
- [ ] Cliente Qt desktop (C++ nativo)
- [ ] Cliente Flutter mobile (Dart)
- [ ] API REST para control remoto

## v1.8.0 — Estabilidad y produccion

- [ ] Tests unitarios: buffer, trigger, decodificadores
- [ ] Test de resistencia: 24h sin perdida de datos
- [ ] CI/CD: GitHub Actions para compilar en ARM/ARM64/x64
- [ ] Package para Raspberry Pi OS (.deb)
- [ ] Dashboard web con historial de capturas

## Bugs

Ver [`BUGS.md`](BUGS.md) para el tracking completo con causas raíz, fixes y estado actual.

**Resumen**: 10 bugs identificados, 8 cerrados, 2 abiertos.

## Mejoras continuas

- [x] **DONE**: Flag `--quiet` / `--verbose` al arrancar + toggle debug/quiet en `server_manager.sh`
- [ ] Agregar `static_assert` para tamanos de struct
- [ ] Reemplazar `std::map` con `std::unordered_map` en clientes
- [ ] Usar memory pool para evitar allocaciones en polling loop
- [ ] Documentar todas las funciones con Doxygen
- [ ] Cubrir bordes: buffer vacio, overflow, clientes lentos
