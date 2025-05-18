# UDP File Transfer with Reliability (Sliding Window Protocol)

This project implements a reliable file transfer protocol over UDP using C++. The client reads and sends a file to the server in chunks, while the server acknowledges received data and reconstructs the file. The implementation simulates real-world packet loss and uses a sliding window mechanism for retransmission and flow control.

## 📦 Features

- Sliding Window protocol with window size 5
- Per-packet retransmission up to 5 times
- Simulated packet/ACK loss via `dropPacket()`
- Timeout-based retransmission using `select()`
- Handles duplicate packets and out-of-order delivery
- Graceful EOF handling and file reconstruction

## 🛠 Requirements

- C++17 or later
- Linux/macOS environment
- Makefile (not provided here, but easily written)

## 🚀 Compilation

```bash
g++ -std=c++17 -o myclient myclient.cpp
g++ -std=c++17 -o myserver myserver.cpp
```

## 🧪 Usage

### Server
```bash
./myserver <port> <lossRate>
```

- `port`: Port to listen on (e.g. `8080`)
- `lossRate`: Simulated packet loss percentage (e.g. `10` means 10%)

### Client
```bash
./myclient <server_ip> <server_port> <input_file_path> <output_file_path_on_server>
```

- Sends a file to the server at `server_ip:server_port`
- Server reconstructs the file at the specified path

## 📂 Example

```bash
# Run server
./myserver 9090 10

# On another terminal
./myclient 127.0.0.1 9090 data.txt received/output.txt
```

## 🧰 Files

| File | Description |
|------|-------------|
| `myclient.cpp` | Client-side code for sending file via UDP |
| `myserver.cpp` | Server-side code for receiving and reconstructing file |
| `client.h` | Header containing `UDPPacket`, `ACKPacket`, and constants |
