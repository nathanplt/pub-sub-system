# Low-Latency Pub/Sub Messaging Bus

A production-quality C++ messaging bus optimized for low p99 latency using ZeroMQ and Boost.Asio. Implements a messaging system with brokerless architecture.

## Architecture

### Design Principles
- **One socket per owning thread**: ZeroMQ sockets are not thread-safe
- **No mutexes on sockets**: Each socket has a single owner thread
- **Allocation-light hot path**: Move semantics and preallocated buffers
- **No heavy work in I/O threads**: CPU work delegated to worker pools

### Publisher Architecture
```
Producer Threads (N)     I/O Thread
┌─────────────────┐     ┌─────────────────┐
│ PUSH (inproc)   │────▶│ PULL (inproc)   │
│                 │     │                 │
│ PUSH (inproc)   │────▶│ PUB (tcp:5556)  │
│                 │     │                 │
│ PUSH (inproc)   │────▶│                 │
└─────────────────┘     └─────────────────┘
```

- **Producer threads**: Each owns a thread-local `PUSH` socket connected to `inproc://ingress`
- **I/O thread**: Owns `PULL` socket (bound to `inproc://ingress`) and `PUB` socket (bound to TCP)
- **Fan-in pattern**: Multiple producers → single I/O thread → external subscribers

### Subscriber Architecture
```
I/O Thread              Worker Pool (N)
┌─────────────────┐     ┌─────────────────┐
│ SUB (tcp:5556)  │────▶│ Worker Thread 1 │
│                 │     │                 │
│                 │     │ Worker Thread 2 │
│                 │     │                 │
│                 │     │ Worker Thread N │
└─────────────────┘     └─────────────────┘
```

- **I/O thread**: Owns `SUB` socket, receives messages, posts to worker pool
- **Worker pool**: `boost::asio::thread_pool` for CPU-intensive message processing
- **No blocking**: I/O thread only does recv/send operations

## Features

- **Low-latency design**: Optimized for p99 latency with minimal allocations
- **Thread-safe publishing**: Thread-local sockets eliminate mutex contention
- **Asynchronous processing**: Worker pool handles CPU work without blocking I/O
- **Comprehensive metrics**: p50/p90/p99 latency, throughput, queue depth
- **Configurable**: thread counts, endpoints, metrics period

## Dependencies

### macOS (Homebrew)
```bash
brew install zeromq cppzmq boost
```

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libzmq3-dev libcppzmq-dev libboost-all-dev cmake build-essential
```

### Windows (vcpkg)
```bash
vcpkg install zeromq cppzmq boost-system
```

## Build

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

## Usage

### Basic Example

**Terminal 1 - Start Subscriber:**
```bash
./sub_pool --sub tcp://127.0.0.1:5556 --workers 6
```

**Terminal 2 - Start Publisher:**
```bash
./pub_mt --pub tcp://*:5556 --producers 8 --messages 50000
```

### Command Line Options

**Publisher (`pub_mt`):**
- `--pub <address>`: Publisher bind address (default: `tcp://*:5556`)
- `--producers <N>`: Number of producer threads (default: 4)
- `--messages <N>`: Messages per producer (default: 10000)
- `--topics <prefix>`: Topic prefix (default: `topic`)

**Subscriber (`sub_pool`):**
- `--sub <address>`: Subscriber connect address (default: `tcp://127.0.0.1:5556`)
- `--workers <N>`: Number of worker threads (default: 4)
- `--topics <list>`: Comma-separated topic list (default: `topic0,topic1,topic2,topic3`)

### Advanced Configuration

```cpp
BusConfig config;
config.pub_bind_addr = "tcp://*:5556";
config.sub_connect_addr = "tcp://127.0.0.1:5556";
config.worker_threads = 8;
config.hwm = 10000;
config.metrics_period = std::chrono::milliseconds(1000);

PublisherBus publisher(config);
SubscriberBus subscriber(config, {"topic1", "topic2"}, message_handler);
```

## Metrics

The system provides comprehensive metrics:

```
METRICS: p50=45μs p90=89μs p99=156μs msgs/sec=125000 processed=50000 dropped=0 queue=0
```