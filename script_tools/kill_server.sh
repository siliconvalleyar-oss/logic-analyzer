#!/bin/bash
pids=$(pgrep logic_server)
if [ -z "$pids" ]; then
    echo "No hay procesos logic_server corriendo"
    exit 0
fi
echo "Procesos encontrados:"
ps -p $pids -o pid,cmd --no-headers
echo
kill -9 $pids
sleep 1
if pgrep logic_server > /dev/null; then
    echo "No se pudieron cerrar todos"
    exit 1
fi
echo "Todos cerrados"
