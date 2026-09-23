# Asynchronous TCP File Server & Client

A high-performance, event-driven TCP file server and interactive terminal client built in C. The architecture leverages Linux `epoll` and non-blocking I/O to handle concurrent client connections efficiently without the overhead of thread context switching.

## Architectural Overview

The server operates on a single-threaded Reactor pattern. By utilizing edge-triggered/level-triggered `epoll` mechanisms, the server multiplexes multiple socket connections, ensuring that I/O operations (like reading large files) never block the main execution loop.

### Core Server Features
*   **Event-Driven Concurrency:** Uses `sys/epoll.h` to manage up to 1024 concurrent events dynamically.
*   **Non-Blocking I/O:** Sockets are strictly configured with `O_NONBLOCK` via `fcntl`, preventing deadlocks during slow client reads.
*   **Chunked Data Streaming:** Files are transmitted in optimized 8192-byte chunks to maintain low memory overhead regardless of total file size.
*   **Zero Memory Leaks:** Verified clean state management for connecting and disconnecting clients.

### Interactive Client Features
*   **ANSI Terminal Interface:** A clean, color-coded CLI menu for seamless navigation.
*   **Dynamic Directory Syncing:** Fetches and displays available files from the server's `./files/` directory in real-time.
*   **Download Management:** Automates local file saving and maintains a persistent `download_history.txt` log for user tracking.

## Build and Execution

Ensure you are operating in a POSIX-compliant Linux environment to utilize the `epoll` API.

### Compilation
Compile both the server and client using the provided `Makefile`:
\`\`\`bash
make all
\`\`\`

### Running the Server
Start the server instance. It will automatically create the necessary `./files/` directory to serve content from.
\`\`\`bash
./server
\`\`\`

### Running the Client
In a separate terminal, launch the client to connect to the server (defaults to `127.0.0.1:8080`).
\`\`\`bash
./client
\`\`\`

## Author
**Pushpendra Singh Bansal**
Systems & Network Software Developer
