# Low-Latency Pub/Sub Messaging Bus

A production-quality C++ messaging bus optimized for low p99 latency using ZeroMQ and Boost.Asio. Implements an ephemeral (at-most-once) messaging system with brokerless architecture.

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
- **Configurable**: HWM, thread counts, endpoints, metrics period
- **Production-ready**: RAII, graceful shutdown, error handling

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

## Performance Tuning

### ZeroMQ Socket Options
- **HWM (High Water Mark)**: Controls buffer sizes (default: 1000)
- **I/O threads**: ZeroMQ context I/O threads (default: 1)
- **Socket types**: PUSH/PULL for fan-in, PUB/SUB for distribution

### Threading
- **Producer threads**: More threads = higher throughput (but diminishing returns)
- **Worker threads**: Match CPU cores for optimal utilization
- **I/O threads**: Usually 1 is sufficient for most workloads

### Memory Management
- **Preallocate buffers**: Avoid allocations in hot path
- **Move semantics**: Use `std::move` for large objects
- **Thread-local storage**: Reduces contention

## Metrics

The system provides comprehensive metrics:

```
METRICS: p50=45μs p90=89μs p99=156μs msgs/sec=125000 processed=50000 dropped=0 queue=0
```

- **Latency percentiles**: p50, p90, p99 in microseconds
- **Throughput**: Messages per second
- **Processing stats**: Total processed, dropped messages
- **Queue depth**: Current work queue size

## Message Format

Messages use ZeroMQ multipart format:
- **Frame 1**: Topic (string)
- **Frame 2**: Payload (bytes)

Payload format for latency measurement:
- **Bytes 0-7**: Timestamp (uint64_t nanoseconds)
- **Bytes 8+**: User data

## Error Handling

- **Graceful shutdown**: Proper cleanup of threads and sockets
- **Exception safety**: RAII ensures resources are cleaned up
- **Error reporting**: Comprehensive error messages and logging
- **Backpressure**: ZeroMQ HWM provides natural backpressure

## Thread Safety

- **PublisherBus::produce()**: Thread-safe, creates thread-local sockets
- **SubscriberBus**: Single I/O thread + worker pool
- **Metrics**: Thread-safe with minimal locking
- **Configuration**: Immutable after construction

## License

MIT License - see LICENSE file for details.