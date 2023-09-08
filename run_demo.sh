#!/bin/bash

build_dir="./build/step-viewer"

# Generate favicon.ico to avoid 404 File not found errors in browser
favicon="${build_dir}/favicon.ico"
printf '\x00\x00\x01\x00\x01\x00' > "${favicon}"
printf '\x10\x10\x01\x00\x20\x00\x00\x00\x00\x04\x00\x00\x16\x00\x00\x00' >> "${favicon}"
for i in $(seq 1 256); do
  printf '\xFF\xFF\xFF\xFF' >> "${favicon}"
done

html_file="./build/step-viewer/index.html"
if [ ! -f "$html_file" ]; then
    echo '<!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <title>StepViewer</title>
    </head>
    <body>
        <script async type="text/javascript" src="StepViewer.js"></script>
    </body>
    </html>' > "$html_file"
fi

cd build/step-viewer
python -m http.server 8989 &
server_pid=$!

cleanup() {
    echo "Stopping server..."
    kill $server_pid
    exit 0
}

trap cleanup SIGINT

echo "Server started. Press Ctrl+C to stop."

xdg-open "http://localhost:8989"

while true; do
    sleep 60
done
