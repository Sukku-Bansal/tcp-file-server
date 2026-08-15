# High-Throughput Multithreaded TCP File Server

## Features
- Event-driven I/O with Linux epoll
- Worker thread pool (4 threads)
- Non-blocking socket handling
- Thread-safe with POSIX mutexes
- Zero memory leaks (Valgrind verified)

## Build
```bash
make

