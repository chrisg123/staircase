#!/bin/bash

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

xdg-open "http://localhost:8989"
