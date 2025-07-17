# Pub-Sub Messaging System

A minimal publish–subscribe messaging demo in modern C++.  
Publishers push serialized messages over a ZeroMQ PUB socket, subscribers connect via SUB sockets, deserialize, and hand messages off to a callback running on a Boost.Asio thread pool. Made this as an exercise for low-latency focused design.

## Features

- **ZeroMQ transport (PUB/SUB)** over TCP endpoints
- **Message abstraction** with `topic` + `content` and simple `topic:content` string serialization
- **Thread-safe publisher send** protected by a mutex
- **Asynchronous subscriber dispatch:** non-blocking receive loop posts callbacks into a Boost.Asio thread pool

## Overview

- `main.cpp` – sets up publisher on `tcp://*:5555`, subscriber on `tcp://localhost:5555`, prints received messages
- `message.h` – `Message` class - topic and content, getters/setters, serialization using `topic:content` format
- `publisher.h` / `publisher.cpp` – ZeroMQ `PUB` socket wrapper - binds to address, mutex-guarded `send()`
- `subscriber.h` / `subscriber.cpp` – ZeroMQ `SUB` socket wrapper - connects to address, non-blocking receive loop, posts to Boost.Asio thread pool

## Build

You’ll need:

- A C++17 (or newer) compiler (`g++`, `clang++`)
- **ZeroMQ** (`libzmq`) and **cppzmq** headers
- **Boost** (for Asio)

Using the Makefile:

```bash
make
./messaging-system
```

## Future plans
- More thorough demo with much greater stress testing
- CLI args for endpoints (--bind, --connect, --count)
- Topic filters
- Multiple topics per subscriber and wildcard match
- Better serialization model (e.g., protobuf)
- More optimizations via multithreading and performance profiling
- Advanced metrics (message throughput, latency dist)