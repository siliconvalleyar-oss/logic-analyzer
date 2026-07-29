//==============================================================================
// sha1.cpp
// SHA-1 hash usando OpenSSL (libcrypto) para handshake WebSocket
// Licencia: MIT
//
// La implementacion es un wrapper sobre OpenSSL SHA1_Init/Update/Final.
// Todo el codigo relevante esta en sha1.h como metodos inline.
//==============================================================================

#include "sha1.h"

// La implementacion de SHA1 esta como metodos inline en sha1.h
// usando las funciones SHA1_Init, SHA1_Update y SHA1_Final de OpenSSL.
// Este archivo existe solo para mantener la estructura de compilacion.
