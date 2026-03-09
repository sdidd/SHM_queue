# Windows Shared Memory API - CreateFileMapping Guide

## Overview
Windows shared memory allows multiple processes to access the same memory region. It's lightning fast (~50ns latency) compared to files (~1ms).

---

## Core Functions

### 1. CreateFileMappingA - Create Shared Memory

```cpp
HANDLE CreateFileMappingA(
    HANDLE hFile,              // File handle or INVALID_HANDLE_VALUE for RAM
    LPSECURITY_ATTRIBUTES attr, // Security (use NULL for default)
    DWORD flProtect,           // Memory protection flags
    DWORD dwMaximumSizeHigh,   // Size upper 32 bits (usually 0)
    DWORD dwMaximumSizeLow,    // Size lower 32 bits (actual size)
    LPCSTR lpName              // Name: "Local\\my_queue"
);
```

**Parameters Explained:**

**`hFile`** - Two modes:
- `INVALID_HANDLE_VALUE` → **Anonymous memory** (pure RAM, fastest)
- Real file handle → **File-backed** (persists to disk)

**`flProtect`** - Memory access:
- `PAGE_READWRITE` → Read/write access (most common)
- `PAGE_READONLY` → Read-only
- `PAGE_EXECUTE_READWRITE` → Can execute code (dangerous)

**`dwMaximumSizeLow`** - Size in bytes:
```cpp
dwMaximumSizeLow = 1024 * 1024;  // 1 MB
dwMaximumSizeLow = 4096;         // 4 KB
```

**`lpName`** - Identifier:
- Format: `"Local\\queue_name"` (local to machine)
- Or: `"Global\\queue_name"` (cross-session, requires admin)
- Must be unique per machine

**Returns:**
- `HANDLE` to mapping object (success)
- `NULL` (failure - use `GetLastError()`)

---

### 2. MapViewOfFile - Get Memory Pointer

```cpp
LPVOID MapViewOfFile(
    HANDLE hFileMappingObject,  // Handle from CreateFileMapping
    DWORD dwDesiredAccess,      // Access mode
    DWORD dwFileOffsetHigh,     // Offset upper 32 bits (usually 0)
    DWORD dwFileOffsetLow,      // Offset lower 32 bits (usually 0)
    SIZE_T dwNumberOfBytesToMap // Size to map (0 = entire region)
);
```

**`dwDesiredAccess`**:
- `FILE_MAP_ALL_ACCESS` → Read/write/execute
- `FILE_MAP_READ` → Read-only
- `FILE_MAP_WRITE` → Write (implies read)

**Returns:**
- Pointer to memory region (success)
- `NULL` (failure)

---

### 3. OpenFileMappingA - Attach to Existing

```cpp
HANDLE OpenFileMappingA(
    DWORD dwDesiredAccess,  // FILE_MAP_ALL_ACCESS
    BOOL bInheritHandle,    // FALSE (don't inherit to child processes)
    LPCSTR lpName           // Same name as CreateFileMapping
);
```

**Use Case:** Consumer process attaches to producer's queue

---

### 4. Cleanup Functions

```cpp
UnmapViewOfFile(pMemory);  // Unmap memory pointer
CloseHandle(hMapping);     // Close mapping handle
```

**Important:** Call in reverse order (unmap, then close)

---

## Complete Example - Producer/Consumer

### Producer.cpp - Creates Shared Memory
```cpp
#include <windows.h>
#include <iostream>
#include <cstring>

struct Message {
    char text[256];
};

int main() {
    const char* QUEUE_NAME = "Local\\MyQueue";
    const size_t SIZE = sizeof(Message) * 100;  // 100 messages
    
    // Step 1: Create shared memory
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // Anonymous (RAM-only)
        NULL,                    // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Size upper 32 bits
        SIZE,                    // Size: 100 messages
        QUEUE_NAME               // Name
    );
    
    if (hMapFile == NULL) {
        std::cerr << "CreateFileMapping failed: " << GetLastError() << std::endl;
        return 1;
    }
    
    // Step 2: Get pointer to memory
    void* pMemory = MapViewOfFile(
        hMapFile,                // Mapping handle
        FILE_MAP_ALL_ACCESS,     // Read/write
        0,                       // Offset high
        0,                       // Offset low
        SIZE                     // Map entire region
    );
    
    if (pMemory == NULL) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Step 3: Write data to shared memory
    Message* messages = static_cast<Message*>(pMemory);
    
    strcpy(messages[0].text, "Hello from producer!");
    strcpy(messages[1].text, "This is message 2");
    strcpy(messages[2].text, "Goodbye!");
    
    std::cout << "Messages written. Press Enter to exit..." << std::endl;
    std::cin.get();  // Keep alive so consumer can read
    
    // Step 4: Cleanup
    UnmapViewOfFile(pMemory);
    CloseHandle(hMapFile);
    
    return 0;
}
```

---

### Consumer.cpp - Reads Shared Memory
```cpp
#include <windows.h>
#include <iostream>

struct Message {
    char text[256];
};

int main() {
    const char* QUEUE_NAME = "Local\\MyQueue";
    const size_t SIZE = sizeof(Message) * 100;
    
    // Step 1: Open existing shared memory
    HANDLE hMapFile = OpenFileMappingA(
        FILE_MAP_ALL_ACCESS,     // Read/write access
        FALSE,                   // Don't inherit
        QUEUE_NAME               // Same name as producer
    );
    
    if (hMapFile == NULL) {
        std::cerr << "OpenFileMapping failed: " << GetLastError() << std::endl;
        std::cerr << "Is producer running?" << std::endl;
        return 1;
    }
    
    // Step 2: Get pointer to memory
    void* pMemory = MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, SIZE
    );
    
    if (pMemory == NULL) {
        std::cerr << "MapViewOfFile failed: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }
    
    // Step 3: Read data from shared memory
    Message* messages = static_cast<Message*>(pMemory);
    
    std::cout << "Reading messages:" << std::endl;
    for (int i = 0; i < 3; i++) {
        std::cout << "Message " << i << ": " << messages[i].text << std::endl;
    }
    
    // Step 4: Cleanup
    UnmapViewOfFile(pMemory);
    CloseHandle(hMapFile);
    
    return 0;
}
```

---

## Memory Layout Example

```
Physical RAM (let's say address 0x12000000)
┌────────────────────────────────────────────┐
│ Shared Memory Region: "Local\MyQueue"     │
├────────────────────────────────────────────┤
│ Message[0]: "Hello from producer!"        │  ← messages[0]
├────────────────────────────────────────────┤
│ Message[1]: "This is message 2"           │  ← messages[1]
├────────────────────────────────────────────┤
│ Message[2]: "Goodbye!"                    │  ← messages[2]
├────────────────────────────────────────────┤
│ Message[3]: (empty)                       │
├────────────────────────────────────────────┤
│ ... (up to Message[99])                   │
└────────────────────────────────────────────┘
         ↑                           ↑
    Producer writes            Consumer reads
   (same physical RAM)        (same physical RAM)
```

**Both processes see the SAME memory** - when producer writes, consumer instantly sees it!

---

## Error Handling

```cpp
DWORD GetLastErrorCode() {
    DWORD error = GetLastError();
    
    switch (error) {
        case ERROR_ALREADY_EXISTS:
            return error; // Queue already exists (OK for OpenFileMapping)
            
        case ERROR_FILE_NOT_FOUND:
            std::cerr << "Queue doesn't exist. Start producer first." << std::endl;
            break;
            
        case ERROR_ACCESS_DENIED:
            std::cerr << "Permission denied. Try 'Local\\' instead of 'Global\\'." << std::endl;
            break;
            
        case ERROR_NOT_ENOUGH_MEMORY:
            std::cerr << "Out of memory. Reduce queue size." << std::endl;
            break;
            
        default:
            std::cerr << "Unknown error: " << error << std::endl;
    }
    
    return error;
}
```

---

## Performance Tips

1. **Use Anonymous Memory** (INVALID_HANDLE_VALUE)
   - Fastest - pure RAM, no disk I/O
   - Dies when all processes exit

2. **Align to Cache Lines**
   ```cpp
   struct alignas(64) Message {  // 64-byte cache line
       char text[56];
       uint64_t timestamp;
   };
   ```

3. **Don't Use Locks** - Use atomics instead:
   ```cpp
   struct QueueHeader {
       std::atomic<uint64_t> writeIndex;
       std::atomic<uint64_t> readIndex;
   };
   ```

4. **Pin to Different CPUs**
   ```cpp
   SetThreadAffinityMask(GetCurrentThread(), 1 << 0);  // CPU 0
   ```

---

## Common Pitfalls

❌ **Forgetting to keep producer alive**
```cpp
// Producer exits → memory destroyed → consumer crashes
```
✅ Use `std::cin.get()` or event to keep alive

❌ **Wrong name format**
```cpp
CreateFileMappingA(..., "MyQueue");  // Wrong - no prefix
```
✅ Use `"Local\\MyQueue"` or `"Global\\MyQueue"`

❌ **Not checking for NULL**
```cpp
void* p = MapViewOfFile(...);
p->data = 42;  // CRASH if p is NULL
```
✅ Always check: `if (p == NULL) { handle_error(); }`

---

## Next Steps

For your lock-free queue, you'll combine:
1. **CreateFileMapping** - create shared region
2. **Atomics** - `std::atomic<uint64_t>` for head/tail
3. **Ring buffer** - circular array with modulo arithmetic
4. **Memory barriers** - `memory_order_acquire/release`

Want me to build the full lock-free implementation?
