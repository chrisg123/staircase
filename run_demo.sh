#!/bin/bash
set -xe

port=8989

if lsof -Pi :$port -s "TCP:LISTEN" -t >/dev/null; then
    echo "Server is already running. Access it here: http://localhost:$port"
    exit 0
fi
script_dir="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

build_dir="${script_dir}/build/step-viewer"

# Generate favicon.ico to avoid 404 File not found errors in browser
favicon="${build_dir}/favicon.ico"
if [ ! -f "$favicon" ]; then
    printf '\x00\x00\x01\x00\x01\x00' >"${favicon}"
    printf '\x10\x10\x01\x00\x20\x00\x00\x00\x00\x04\x00\x00\x16\x00\x00\x00' >>"${favicon}"
    for i in $(seq 1 256); do
        printf '\xFF\xFF\xFF\xFF' >>"${favicon}"
    done
fi

html_file="${script_dir}/web/index.html"
cp "${html_file}" "${build_dir}/index.html"

cd build/step-viewer
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
