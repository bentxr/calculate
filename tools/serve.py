#!/usr/bin/env python3
"""Serves a directory with the headers Qt's multithread WebAssembly build needs:
cross-origin isolation (for SharedArrayBuffer) and the wasm MIME type.
Usage: tools/serve.py build/wasm [port]  ->  http://127.0.0.1:8765/calculate.html"""
import functools
import http.server
import sys


class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()


Handler.extensions_map[".wasm"] = "application/wasm"
directory = sys.argv[1] if len(sys.argv) > 1 else "."
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8765
print(f"http://127.0.0.1:{port}/calculate.html (serving {directory})")
http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=directory)).serve_forever()
