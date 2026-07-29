#!/bin/bash
while true; do
    echo "=== Logic Server Manager ==="
    echo "1) Iniciar manual (./server/logic_server 8080)"
    echo "2) Iniciar con systemd (servicio permanente)"
    echo "3) Detener todo (systemd + manual)"
    echo "4) Salir"
    read -p "Opcion: " opt
    case $opt in
        1) ./server/logic_server 8080; break ;;
        2) sudo systemctl enable logic-analyzer && sudo systemctl start logic-analyzer && echo "OK"; break ;;
        3)
            sudo systemctl stop logic-analyzer
            sudo killall -9 logic_server 2>/dev/null
            echo "Detenido"; break ;;
        4) exit 0 ;;
        *) echo "Opcion invalida" ;;
    esac
done
