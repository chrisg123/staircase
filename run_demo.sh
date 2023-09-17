#!/bin/bash
set -xe

port=8989

if lsof -Pi :$port -s "TCP:LISTEN" -t >/dev/null; then
    echo "Server is already running. Access it here: http://localhost:$port"
    exit 0
fi
script_dir="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

build_dir="${script_dir}/build/staircase"

cd build/staircase
python -m http.server $port &
server_pid=$!

cleanup() {
    echo "Stopping server..."
    kill $server_pid
    exit 0
}

trap cleanup SIGINT

echo "Server started. Press Ctrl+C to stop."

xdg-open "http://localhost:$port"

while true; do
    sleep 60
done
