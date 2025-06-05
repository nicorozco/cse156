import subprocess
import threading

# Configuration
NUM_CLIENTS = 50
PROXY_IP = "127.0.0.1"
PROXY_PORT = "9090"
TARGET_URL = "http://www.example.com/index.html"

def run_client(client_id):
    try:
        cmd = [
            "./myclient",
            PROXY_IP,
            PROXY_PORT,
            TARGET_URL
        ]
        print(f"[Client {client_id}] Starting...")
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(f"[Client {client_id}] Finished with status {result.returncode}")
        if result.stdout:
            print(f"[Client {client_id}] Output:\n{result.stdout}")
        if result.stderr:
            print(f"[Client {client_id}] Error:\n{result.stderr}")
    except Exception as e:
        print(f"[Client {client_id}] Exception: {e}")

# Launch all clients
threads = []
for i in range(NUM_CLIENTS):
    t = threading.Thread(target=run_client, args=(i + 1,))
    threads.append(t)
    t.start()

# Wait for all to finish
for t in threads:
    t.join()

print("\n✅ All clients finished.")
