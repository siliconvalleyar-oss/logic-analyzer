#!/bin/bash
pids=$(pgrep logic_server)
if [ -z "$pids" ]; then
    echo "No hay procesos logic_server corriendo"
    exit 0
fi
echo "Procesos encontrados:"
ps -p $pids -o pid,cmd --no-headers
echo
sudo kill -9 $pids
sleep 1
pids=$(pgrep logic_server)
if [ -n "$pids" ]; then
    echo "No se pudieron cerrar:"
    ps -p $pids -o pid,cmd --no-headers
    exit 1
fi
echo "Todos cerrados"
