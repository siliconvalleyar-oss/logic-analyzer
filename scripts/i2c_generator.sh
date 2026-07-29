#!/bin/bash
#===============================================================================
# i2c_generator.sh — Generador de senales I2C para probar el decodificador
#
# Genera transacciones I2C reales en GPIO2 (SDA) y GPIO3 (SCL) usando
# bit-bang con pinctrl. Produce senales lentas (~5kHz) para que el
# analizador logico las capture facilmente.
#
# Transaccion generada:
#   START → Addr 0x42 (W) → ACK → Data 0xAB → ACK → Data 0xCD → NACK → STOP
#   1 segundo de pausa
#   START → Addr 0x42 (R) → ACK → Data 0x55 → NACK → STOP
#   1 segundo de pausa
#   ... se repite
#
# Usage:
#   ./scripts/i2c_generator.sh                # foreground (Ctrl+C para salir)
#   ./scripts/i2c_generator.sh --daemon       # background
#   ./scripts/i2c_generator.sh --stop         # detener
#
# Pines:
#   GPIO2 = SDA (data)
#   GPIO3 = SCL (clock)
#===============================================================================

set -euo pipefail

SCRIPT_NAME="i2c_generator"
PID_FILE="/tmp/${SCRIPT_NAME}.pid"
SDA=2
SCL=3

# === COLORS ===
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
ok()   { echo -e "${GREEN}[OK]${NC}   $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
err()  { echo -e "${RED}[ERROR]${NC} $1"; }
info() { echo -e "${CYAN}[INFO]${NC} $1"; }

# === UTILITIES ===
is_rpi() { grep -iq "Raspberry Pi\|BCM\|bcm2835" /proc/cpuinfo 2>/dev/null && return 0; return 1; }
has_pinctrl() { command -v pinctrl &>/dev/null; }

# Funciones pinctrl simplificadas (cada llamada hace UNA operacion)
sda() { pinctrl set "$SDA" "$1" 2>/dev/null || true; }
scl() { pinctrl set "$SCL" "$1" 2>/dev/null || true; }

# Microsleep usando bash (aproximado)
# Microsegundos con fallback a perl si usleep no existe
# Microsegundos (soporta usleep, perl, o sleep decimal)
usleep() {
    local t=$1
    if command -v usleep &>/dev/null; then
        command usleep "$t" 2>/dev/null || true
    elif command -v perl &>/dev/null; then
        perl -e "usleep($t)" 2>/dev/null || true
    else
        # Fallback con sleep decimal
        sleep "0.$(printf '%06d' $t)" 2>/dev/null || true
    fi
}

# === I2C BIT-BANG PRIMITIVAS ===
# Periodo base: 200µs para ~5kHz SCL
# Mitad de periodo para los delays
HALF_BIT=100  # microsegundos

i2c_init() {
    info "Inicializando GPIO$SDA (SDA) y GPIO$SCL (SCL) como outputs..."
    # Configurar como outputs (cada operacion por separado)
    scl op
    usleep 50
    scl dl
    usleep 50
    sda op
    usleep 50
    sda dl
    usleep 50
    # Estado idle: ambos HIGH
    sda dh
    usleep "$HALF_BIT"
    scl dh
    usleep "$HALF_BIT"
    ok "GPIO$SDA=SDA, GPIO$SCL=SCL — idle HIGH"
}

i2c_cleanup() {
    # Restaurar a input (cada operacion por separado)
    pinctrl set "$SDA" ip 2>/dev/null || true
    pinctrl set "$SCL" ip 2>/dev/null || true
    # Deshabilitar pulls
    pinctrl set "$SDA" pn 2>/dev/null || true
    pinctrl set "$SCL" pn 2>/dev/null || true
    ok "GPIOs restaurados a input"
}

i2c_start() {
    # START: SDA goes LOW while SCL is HIGH
    sda dl
    usleep "$HALF_BIT"
    scl dl
    usleep "$HALF_BIT"
}

i2c_stop() {
    # STOP: SDA goes HIGH while SCL is HIGH
    sda dl
    usleep "$HALF_BIT"
    scl dh
    usleep "$HALF_BIT"
    sda dh
    usleep "$HALF_BIT"
}

i2c_write_bit() {
    local bit=$1
    # SCL is LOW already
    if [ "$bit" -eq 1 ]; then
        sda dh
    else
        sda dl
    fi
    usleep "$HALF_BIT"
    # Rising edge SCL — data is sampled here
    scl dh
    usleep "$HALF_BIT"
    # Falling edge SCL
    scl dl
    usleep "$HALF_BIT"
}

i2c_read_ack() {
    # Release SDA (set HIGH, then change to input to let device pull low)
    sda dh
    usleep "$HALF_BIT"
    scl dh
    usleep "$HALF_BIT"
    # In a real system, we'd read the SDA pin here.
    # For simulation, we assume ACK (SDA=0) for address and data bytes,
    # NACK for the last byte.
    scl dl
    usleep "$HALF_BIT"
    return 0  # 0 = ACK
}

i2c_write_byte() {
    local byte=$1
    local ack=${2:-1}  # 1=ACK (SDA LOW), 0=NACK (SDA HIGH)
    for i in $(seq 7 -1 0); do
        local bit=$(( (byte >> i) & 1 ))
        i2c_write_bit "$bit"
    done
    # ACK/NACK: se coloca ANTES del rising edge de SCL
    # El decodificador I2C del analizador muestrea SDA en flanco ascendente
    if [ "$ack" -eq 1 ]; then
        sda dl  # ACK: SDA LOW antes del rising edge
    else
        sda dh  # NACK: SDA HIGH antes del rising edge
    fi
    usleep "$HALF_BIT"
    scl dh  # rising edge — SDA ya tiene el nivel correcto
    usleep "$HALF_BIT"
    scl dl
    usleep "$HALF_BIT"
}

# === TRANSMITIR TRANSACCION I2C ===
send_transaction() {
    local addr=$1       # Direccion de 7 bits (ej: 0x42)
    local rw=$2         # 0=write, 1=read
    shift 2
    local data=("$@")   # Bytes de datos

    i2c_start
    # Address byte: addr << 1 | rw
    local addr_byte=$(( (addr << 1) | rw ))
    i2c_write_byte "$addr_byte"

    if [ "$rw" -eq 0 ]; then
        # Write: todos los bytes con ACK (simula slave respondiendo)
        for d in "${data[@]}"; do
            i2c_write_byte "$d" 1  # ACK
        done
    else
        # Read: 1 byte con NACK al final (master senaliza fin)
        for i in $(seq 7 -1 0); do
            sda dh
            usleep "$HALF_BIT"
            scl dh
            usleep "$HALF_BIT"
            scl dl
            usleep "$HALF_BIT"
        done
        # NACK after read byte (SDA HIGH antes del rising edge)
        sda dh
        usleep "$HALF_BIT"
        scl dh
        usleep "$HALF_BIT"
        scl dl
        usleep "$HALF_BIT"
    fi

    i2c_stop
}

# === BUCLE PRINCIPAL ===
generate_i2c_traffic() {
    trap 'i2c_cleanup; exit 0' SIGTERM SIGINT EXIT

    i2c_init

    info "Generando trafico I2C en GPIO$SDA (SDA) / GPIO$SCL (SCL)..."
    echo ""
    echo "  Transaccion 1: Addr 0x42 (W) → 0xAB → 0xCD  [cada 2s]"
    echo "  Transaccion 2: Addr 0x42 (R) → 0x55            [cada 2s]"
    echo ""

    local count=0
    while true; do
        count=$(( count + 1 ))
        echo -ne "${CYAN}[${count}]${NC} Transaccion Write: Addr 0x42 → 0xAB → 0xCD ... "

        # Transaction 1: Write 0xAB 0xCD
        send_transaction 0x42 0 0xAB 0xCD
        echo -e "${GREEN}OK${NC}"

        sleep 1

        echo -ne "${CYAN}[${count}]${NC} Transaccion Read:  Addr 0x42 → ... "

        # Transaction 2: Read
        send_transaction 0x42 1
        echo -e "${GREEN}OK${NC}"

        echo ""
        sleep 1
    done
}

# === COMANDOS ===
usage() {
    cat <<EOF
Generador de senales I2C para probar el decodificador del Logic Analyzer

Usage: ${0##*/} [options]

Options:
  --daemon    Iniciar en background
  --stop      Detener el daemon
  --help      Mostrar ayuda

Pines:
  GPIO2 = SDA
  GPIO3 = SCL

Transaccion:
  Addr 0x42 (W) → 0xAB → 0xCD → STOP → Addr 0x42 (R) → 0x55 → STOP
  (se repite cada ~2 segundos)

Velocidad: ~5kHz SCL (200µs por bit)
EOF
    exit 0
}

do_daemon() {
    if [[ -f "$PID_FILE" ]]; then
        local old_pid
        old_pid=$(cat "$PID_FILE" 2>/dev/null || echo "")
        if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
            err "Ya hay una instancia corriendo (PID $old_pid)"
            exit 1
        fi
        rm -f "$PID_FILE"
    fi
    info "Iniciando I2C generator en background..."
    nohup bash "$0" --foreground </dev/null > /tmp/i2c_generator.log 2>&1 &
    local pid=$!
    echo "$pid" > "$PID_FILE"
    ok "I2C generator iniciado (PID $pid)"
    info "Log: /tmp/i2c_generator.log"
}

do_stop() {
    if [[ ! -f "$PID_FILE" ]]; then
        warn "No se encontro PID file"
        i2c_cleanup
        exit 1
    fi
    local pid
    pid=$(cat "$PID_FILE" 2>/dev/null || echo "")
    if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
        info "Deteniendo I2C generator (PID $pid)..."
        kill "$pid" 2>/dev/null || true
        sleep 1
        kill -0 "$pid" 2>/dev/null && kill -9 "$pid" 2>/dev/null || true
        ok "Detenido"
    fi
    i2c_cleanup
    rm -f "$PID_FILE"
}

case "${1:-}" in
    --help|-h) usage ;;
    --daemon)  do_daemon ;;
    --stop)    do_stop ;;
    --foreground) generate_i2c_traffic ;;
    "")
        if ! is_rpi; then
            echo "⚠️  No se detecto Raspberry Pi — solo modo simulacion"
            echo "   Copiar script a la Pi y ejecutar alli."
            cat << 'SIM'
Esperado en Pi:
  GPIO2 (SDA) ┐┌──────┐┌──────┐┌──────┐┌────
              ┘└──────┘└──────┘└──────┘└────
  GPIO3 (SCL) ┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──┐┌──
              ┘└──┘└──┘└──┘└──┘└──┘└──┘└──
  Trama: START | Addr 0x42 W | ACK | Data 0xAB | ACK | Data 0xCD | NACK | STOP
SIM
            exit 0
        fi
        generate_i2c_traffic
        ;;
    *) err "Opcion desconocida: $1"; usage ;;
esac
