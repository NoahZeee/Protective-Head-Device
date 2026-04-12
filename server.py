import asyncio
import socket
import threading
import json
from http.server import HTTPServer, SimpleHTTPRequestHandler
import websockets

# Config
TCP_HOST  = "0.0.0.0"
TCP_PORT  = 11111
WS_PORT   = 8765
HTTP_PORT = 8080

# Shared state 
helmet = {
    "id":        "H1",
    "status":    "OK",      # OK | BACKUP | EMERGENCY
    "connected": False,
}

ws_clients = set()   # connected browser tabs
tcp_conn   = None    # the one ESP32 TCP connection
tcp_lock   = threading.Lock()
ws_loop    = None

# Helpers 
async def broadcast(data: dict):
    if ws_clients:
        msg = json.dumps(data)
        await asyncio.gather(*[ws.send(msg) for ws in ws_clients], return_exceptions=True)

def broadcast_sync(data: dict):
    if ws_loop:
        asyncio.run_coroutine_threadsafe(broadcast(data), ws_loop)

# TCP thread  talks to ESP32
def tcp_server_thread():
    global tcp_conn

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((TCP_HOST, TCP_PORT))
    srv.listen(1)
    print(f"[TCP]  Listening on port {TCP_PORT} ...")

    while True:
        conn, addr = srv.accept()
        with tcp_lock:
            tcp_conn = conn
        helmet["connected"] = True
        print(f"[TCP]  Helmet connected from {addr}")
        broadcast_sync({"type": "helmet_connected", "helmet": dict(helmet)})

        buffer = ""
        try:
            while True:
                chunk = conn.recv(1024).decode(errors="ignore")
                if not chunk:
                    break
                buffer += chunk
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    print(f"[TCP]  Received: {line}")

                    if line == "EMERGENCY":
                        helmet["status"] = "EMERGENCY"
                    elif line == "BACKUP":
                        helmet["status"] = "BACKUP"
                    else:
                        helmet["status"] = "OK"

                    broadcast_sync({"type": "status_update", "helmet": dict(helmet)})

        except Exception as e:
            print(f"[TCP]  Error: {e}")
        finally:
            with tcp_lock:
                tcp_conn = None
            helmet["connected"] = False
            helmet["status"]    = "OK"
            broadcast_sync({"type": "helmet_disconnected", "helmet": dict(helmet)})
            print("[TCP]  Helmet disconnected")

# WebSocket handler  talks to browser 
async def ws_handler(websocket):
    ws_clients.add(websocket)
    print(f"[WS]   Browser connected  (total: {len(ws_clients)})")
    await websocket.send(json.dumps({"type": "init", "helmet": dict(helmet)}))

    try:
        async for raw in websocket:
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue

            if msg.get("type") == "send_message":
                text = msg.get("text", "").strip()
                if text:
                    with tcp_lock:
                        if tcp_conn:
                            try:
                                tcp_conn.sendall((text + "\n").encode())
                                print(f"[WS]   Sent to helmet: {text}")
                            except Exception as e:
                                print(f"[WS]   Send error: {e}")
                        else:
                            print("[WS]   No helmet connected – dropped")

    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        ws_clients.discard(websocket)
        print(f"[WS]   Browser disconnected (total: {len(ws_clients)})")

# HTTP server – serves index.html 
def http_server_thread():
    import os
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    httpd = HTTPServer(("0.0.0.0", HTTP_PORT), SimpleHTTPRequestHandler)
    print(f"[HTTP] Serving → http://localhost:{HTTP_PORT}")
    httpd.serve_forever()

#  Entry point
async def main():
    global ws_loop
    ws_loop = asyncio.get_event_loop()

    threading.Thread(target=tcp_server_thread, daemon=True).start()
    threading.Thread(target=http_server_thread, daemon=True).start()

    async with websockets.serve(ws_handler, "0.0.0.0", WS_PORT):
        print(f"[WS]   WebSocket on port {WS_PORT}")
        print("=" * 50)
        print(f"  Open → http://localhost:{HTTP_PORT}")
        print("=" * 50)
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
