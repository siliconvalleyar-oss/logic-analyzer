//==============================================================================
// version.h
// Version del servidor del analizador logico
// Licencia: MIT
//==============================================================================

#ifndef LOGIC_VERSION_H
#define LOGIC_VERSION_H

/**
 * @def LOGIC_VERSION_MAJOR
 * Version mayor del servidor. Se incrementa con cambios
 * incompatibles en la API o el protocolo WebSocket.
 */
#define LOGIC_VERSION_MAJOR 1

/**
 * @def LOGIC_VERSION_MINOR
 * Version menor. Se incrementa con nuevas funcionalidades
 * compatibles hacia atras.
 */
#define LOGIC_VERSION_MINOR 1

/**
 * @def LOGIC_VERSION_PATCH
 * Patch level. Se incrementa con correcciones de bugs
 * o mejoras menores de rendimiento.
 */
#define LOGIC_VERSION_PATCH 0

/**
 * @def LOGIC_VERSION_STRING
 * Version completa como string para mostrar en --version
 * y en los mensajes de log de inicio.
 * Formato: "MAJOR.MINOR.PATCH"
 */
#define LOGIC_VERSION_STRING "1.1.0"

#endif // LOGIC_VERSION_H
