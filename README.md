# shm_queue

**Single-header C++ IPC library using shared memory and file-backed queues (Windows)**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](https://github.com)

Pass data between processes with minimal overhead. Drop `include/shm_queue.hpp` into your project—no build system required.

---

## Two Queue Types

| | `SHMQueue<T>` | `FileQueue<std::string>` |
|-|---------------|--------------------------|
| **Backend** | Windows shared memory (`CreateFileMapping`) | Files on disk (`.txt` + `.index`) |
| **Speed** | Fast (~microseconds) | Slow (~milliseconds) |
| **Type** | POD structs or `std::string` | `std::string` only |
| **Persistence** | Dies when all handles close | Survives process crashes |
| **Use for** | High-throughput IPC | Debugging, crash-safe logging |

---

## Requirements

- Windows
- C++17 (`-std=c++17`)
- No external dependencies

---

## Installation

```bash
# Copy the single header into your project
cp include/shm_queue.hpp your_project/include/
```

---

## SHMQueue — Shared Memory Queue

### Producer

```cpp
#include "shm_queue.hpp"

struct SensorData {
    double id;
    double temperature;
    double humidity;
    char location[50];  // Fixed-size arrays only — no std::string members!
    int timestamp;
};

int main() {
    shm_queue::SHMQueue<SensorData> queue;
    queue.create_shm_queue("sensor_queue", "local", 5);  // name, type, capacity

    SensorData data = {1.0, 22.5, 45.0, "Room A", 1234567890};
    queue << data;  // Push — throws if full
}
```

### Consumer

```cpp
#include "shm_queue.hpp"

struct SensorData { /* same definition as producer */ };

int main() {
    shm_queue::SHMQueue<SensorData> queue;
    queue.attach_to_shm_queue("sensor_queue", "local");  // Attach to existing queue

    SensorData data;
    queue >> data;  // Pop — throws if empty
    std::cout << "Temp: " << data.temperature << "\n";
}
```

### Type Constraint

`T` must be **`std::string`** or a **POD type** (trivially copyable + standard layout):

```cpp
// ✅ Valid
struct Good {
    int id;
    float value;
    char name[64];
};

// ❌ Invalid — contains std::string, which has an internal pointer
struct Bad {
    std::string name;
};
```

For strings in shared memory, use `SHMQueue<std::string>`:

```cpp
shm_queue::SHMQueue<std::string> q;
q.create_shm_queue("text_queue", "local", 10);        // uses 256-byte slots by default
q.create_shm_queue("text_queue", "local", 10, 512);   // custom slot size

q << std::string("hello");
std::string msg;
q >> msg;
```

### API

```cpp
// Create (producer side)
queue.create_shm_queue(name, "local", capacity);
queue.create_shm_queue(name, "local", capacity, itemSize);  // custom item size for strings

// Attach (consumer side)
queue.attach_to_shm_queue(name, "local");

// Push / pop
queue << value;     // throws std::runtime_error if full
queue >> value;     // throws std::runtime_error if empty

// Public members
queue.name_;        // std::string — the queue name
```

---

## FileQueue — File-Backed Queue

Uses two files: `<name>.txt` (data) and `<name>.index` (read position). Only supports `std::string`.

### Producer

```cpp
#include "shm_queue.hpp"

int main() {
    shm_queue::FileQueue<std::string> q("messages");  // creates messages.txt + messages.index

    q << "hello\n";
    q << "world\n";
}
```

### Consumer

```cpp
#include "shm_queue.hpp"

int main() {
    shm_queue::FileQueue<std::string> q("messages");

    std::string line;
    q >> line;              // reads next line; returns empty string if no new data
    if (!line.empty()) {
        std::cout << line << "\n";
    }
}
```

### API

```cpp
FileQueue<std::string> q(filename);     // opens/creates the queue files

q << "some string";                     // append to queue
q >> str;                               // read next line (empty if nothing new)
```

---

## Building the Examples

```powershell
# Windows (from repo root)
.\build.ps1

# Or manually
g++ -std=c++17 -I include examples/producer.cpp    -o producer.exe
g++ -std=c++17 -I include examples/consumer.cpp   -o consumer.exe

# Struct example (single-process read/write demo)
g++ -std=c++17 -I include examples/struct_example.cpp -o struct_example.exe

# FileQueue examples
g++ -std=c++17 -I include examples/FileQueue/producer_fileQueue.cpp -o producer_file.exe
g++ -std=c++17 -I include examples/FileQueue/consumer_fileQueue.cpp -o consumer_file.exe
```

Run producer first, then consumer in a separate terminal:

```powershell
# Terminal 1
.\producer.exe

# Terminal 2
.\consumer.exe
```

---

## Common Pitfalls

**Queue full / empty throws** — `<<` and `>>` throw `std::runtime_error`. Wrap in try/catch:

```cpp
try {
    queue >> data;
} catch (const std::exception& e) {
    // queue was empty
}
```

**Non-POD types will fail to compile:**

```cpp
static_assert(std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>,
    "T must be POD or std::string");
```

**Producer must create before consumer attaches** — `attach_to_shm_queue` calls `OpenFileMappingA` which fails if the mapping doesn't exist yet.

**Queue is destroyed when all handles close** — there is no explicit `destroy()` call; cleanup is automatic via the destructor (`UnmapViewOfFile` + `CloseHandle`).

---

## License

MIT — see [LICENSE](LICENSE).
