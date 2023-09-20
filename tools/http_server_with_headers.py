import argparse
from http.server import SimpleHTTPRequestHandler, HTTPServer

class CORSRequestHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        SimpleHTTPRequestHandler.end_headers(self)

def main():
    parser = argparse.ArgumentParser(description="Run a simple HTTP server with CORS headers.")
    parser.add_argument('--port', type=int, default=8989, help='Port to run the server on.')
    args = parser.parse_args()


    httpd = HTTPServer(('localhost', args.port), CORSRequestHandler)
    print(f"Server running at http://localhost:{args.port}")
    httpd.serve_forever()
    return 0

if __name__ == '__main__':
    main()
