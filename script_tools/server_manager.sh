#!/bin/bash
menu() {
    echo "=== Logic Server Manager ==="
    echo "1) Iniciar manual (./server/logic_server 8080)"
    echo "2) Iniciar systemd"
    echo "3) Detener (manual + systemd)"
    echo "4) Deshabilitar systemd (no arranca solo)"
    echo "5) Estado"
    echo "6) Salir"
    read -p "Opcion: " opt
    case $opt in
        1) ./server/logic_server 8080 ;;
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
