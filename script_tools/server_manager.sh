#!/bin/bash
menu() {
    echo "=== Logic Server Manager ==="
    echo "1) Iniciar manual (./server/logic_server 8080)"
    echo "2) Iniciar con systemd (servicio permanente)"
    echo "3) Detener todo (manual + systemd)"
    echo "4) Estado"
    echo "5) Salir"
    read -p "Opcion: " opt
    case $opt in
        1) ./server/logic_server 8080 ;;
        2)
            sudo systemctl enable logic-analyzer
            sudo systemctl start logic-analyzer
            echo "Systemd iniciado"
            ;;
        3)
            sudo systemctl stop logic-analyzer 2>/dev/null
            sudo killall -9 logic_server 2>/dev/null
            echo "Detenido"
            ;;
        4)
            echo "--- Procesos ---"
            pgrep -a logic_server || echo "(ninguno)"
            echo "--- Systemd ---"
            sudo systemctl is-active logic-analyzer 2>/dev/null || echo "inactivo"
            echo "--- Hilos ---"
            pid=$(pgrep logic_server | head -1)
            if [ -n "$pid" ]; then
                ps -T -p $pid -o pid,tid,comm --no-headers
            fi
            ;;
        5) exit 0 ;;
        *) echo "Opcion invalida" ;;
    esac
    echo ""
    menu
}
menu
