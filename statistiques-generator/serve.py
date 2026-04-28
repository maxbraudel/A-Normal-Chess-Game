from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os
import webbrowser


def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    port = int(os.environ.get("ANCG_STATS_PORT", "8765"))
    bind_host = os.environ.get("ANCG_STATS_HOST", "127.0.0.1")

    os.chdir(project_root)

    url = f"http://{bind_host}:{port}/statistiques-generator/index.html"
    print(f"Serving project root: {project_root}")
    print(f"Stats viewer URL: {url}")

    if os.environ.get("ANCG_STATS_OPEN_BROWSER", "1") != "0":
        try:
            webbrowser.open(url)
        except Exception:
            pass

    server = ThreadingHTTPServer((bind_host, port), SimpleHTTPRequestHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()