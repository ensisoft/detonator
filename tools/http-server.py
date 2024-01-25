import http.server
import socketserver
import os
import time

# Set the port you want to use for the web server
port = 8000

# Define a custom request handler to enable SharedArrayBuffer
class CustomRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        #if self.path.endswith(".js"):
        #    print('set content type to text/javascript')
        #    self.send_header('Content-Type', 'text/javascript')

        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

    def guess_type(self, path):
        if path.endswith('.html'):
            return "text/html"
        if path.endswith('.js'):
            return "text/javascript"

        return super().guess_type(path)

# Change to the directory where your files are located
web_dir = "."
os.chdir(web_dir)

# Start the web server
with socketserver.TCPServer(("", port), CustomRequestHandler) as httpd:
    print("Serving at port", port)
    httpd.serve_forever()