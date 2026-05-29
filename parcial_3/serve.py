#!/usr/bin/env python3
"""HTTP server para el dashboard del Mini-SO.

- Sirve los archivos estaticos (dashboard.html, estado.json) en /
- Acepta POST /cmd con JSON {"verb": "X", "args": [...]} y lo escribe
  en comandos.txt para que el kernel (corriendo en --remote) lo procese.

Lanzamiento:
    python serve.py
    (luego abrir http://localhost:8000/dashboard.html)
"""
import json
import os
import threading
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler

PORT = 8000
CMD_FILE = "comandos.txt"
cmd_lock = threading.Lock()


class Handler(SimpleHTTPRequestHandler):
    def _json(self, code, payload):
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(json.dumps(payload).encode("utf-8"))

    def do_POST(self):
        if self.path == "/cmd":
            length = int(self.headers.get("Content-Length", 0))
            raw = self.rfile.read(length).decode("utf-8", errors="replace")
            try:
                data = json.loads(raw)
            except Exception as e:
                return self._json(400, {"ok": False, "error": "bad json: " + str(e)})

            verb = str(data.get("verb", "")).strip().upper()
            args = data.get("args", []) or []
            if not verb:
                return self._json(400, {"ok": False, "error": "empty verb"})

            # Sanitizar: ningun arg puede contener tab o newline (los usamos como sep)
            parts = [verb]
            for a in args:
                s = str(a).replace("\t", " ").replace("\n", " ").replace("\r", " ")
                parts.append(s)
            line = "\t".join(parts) + "\n"

            with cmd_lock:
                with open(CMD_FILE, "a", encoding="utf-8") as f:
                    f.write(line)
            print(f"[serve] enqueued: {verb} {args}")
            return self._json(200, {"ok": True, "verb": verb, "args": args})

        self._json(404, {"ok": False, "error": "not found"})

    def end_headers(self):
        # JSON debe leerse fresco
        if self.path.startswith("/estado.json"):
            self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):
        # silenciar logs ruidosos de GET; mantener solo POSTs por print arriba
        if args and isinstance(args[0], str) and args[0].startswith("POST"):
            super().log_message(fmt, *args)

    def do_GET(self):
        # evitar 404 ruidoso por el favicon que pide el browser
        if self.path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
            return
        super().do_GET()


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    httpd = ThreadingHTTPServer(("", PORT), Handler)
    print(f"Mini-SO dashboard en http://localhost:{PORT}/dashboard.html")
    print(f"Comandos -> {CMD_FILE} (el kernel debe correr con --remote)")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[serve] detenido por usuario")
    httpd.server_close()


if __name__ == "__main__":
    main()
