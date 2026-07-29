#!/bin/bash

DEBUG_FLAG=""

menu() {
    local debug_str
    if [ -n "$DEBUG_FLAG" ]; then
        debug_str="SIN debug (quiet)"
    else
        debug_str="CON debug"
    fi
    echo "=== Logic Server Manager ==="
    echo "1a) Iniciar manual (./server/logic_server 8080) — $debug_str"
    echo "1b) Alternar debug / quiet"
    echo "2) Iniciar systemd"
    echo "3) Detener (manual + systemd)"
    echo "4) Deshabilitar systemd (no arranca solo)"
    echo "5) Estado"
    echo "6) Salir"
    read -p "Opcion: " opt
    case $opt in
        1a|1)
            if [ -f ./server/logic_server ]; then
                echo "Iniciando: ./server/logic_server 8080 $DEBUG_FLAG"
                ./server/logic_server 8080 $DEBUG_FLAG
            else
                echo "Binario no encontrado en ./server/logic_server"
            fi
            ;;
        1b)
            if [ -z "$DEBUG_FLAG" ]; then
                DEBUG_FLAG="--quiet"
                echo "Modo cambiado a: SIN debug (quiet)"
            else
                DEBUG_FLAG=""
                echo "Modo cambiado a: CON debug"
            fi
            ;;
        2)
            sudo systemctl enable --now logic-analyzer
            echo "Systemd iniciado"
            ;;
        3)
            sudo systemctl stop logic-analyzer
            sleep 1
            left=$(pgrep logic_server)
            if [ -n "$left" ]; then
                sudo kill -9 $left 2>/dev/null
            fi
            echo "Detenido"
            ;;
        4)
            sudo systemctl disable logic-analyzer
            echo "Systemd deshabilitado"
            ;;
        5)
            echo "--- Procesos ---"
            p=$(pgrep logic_server)
            if [ -n "$p" ]; then
                ps -p $p -o pid,cmd --no-headers
                echo "--- Hilos ---"
                ps -T -p $p -o pid,tid,comm --no-headers
            else
                echo "(ninguno)"
            fi
            echo "--- Systemd ---"
            sudo systemctl is-active logic-analyzer 2>/dev/null || echo "inactivo"
            sudo systemctl is-enabled logic-analyzer 2>/dev/null || echo "deshabilitado"
            ;;
        6) exit 0 ;;
        *) echo "Opcion invalida" ;;
    esac
    echo ""
    menu
}
menu
