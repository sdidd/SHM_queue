#pragma once

#include <fstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <iostream>
#include <windows.h>  // For CreateFileMapping, MapViewOfFile

namespace shm_queue {

    // File-based queue - persistent, crash-safe, stores data on disk
    // Use for: sensitive data, crash recovery, debugging
    // Trade-off: Slower (~1ms latency) but survives process crashes
    template<typename T>
    class FileQueue {
        private:
            // Ensure T is a string type for simplicity
            static_assert(std::is_same<T, std::string>::value, "FileQueue only supports std::string for now");
            double index_ = 0; // To track the current read/write position in the file
            std::string filename_;

        public:
            FileQueue(const std::string& filename) : filename_(filename) {
                // Create the file if it doesn't exist
                std::ofstream ofs(filename_ + ".txt", std::ios::app);
                std::ofstream idx(filename_ + ".index", std::ios::app);
                if (!ofs) {
                    throw std::runtime_error("Failed to create queue file");
                }
                if (!idx) {
                    throw std::runtime_error("Failed to create index file");
                }
            }

            // Write string data to the queue
            FileQueue& operator<<(const std::string& data) {
                std::ofstream ofs(filename_ + ".txt", std::ios::app);
                if (!ofs) {
                    throw std::runtime_error("Failed to open queue file for writing");
                }
                ofs << data;
                if (!ofs) {
                    throw std::runtime_error("Failed to write to queue file");
                }
                return *this;
            }

            // Write C-string data to the queue
            FileQueue& operator<<(const char* data) {
                return (*this) << std::string(data);
            }

            FileQueue& operator<<(const char data) {
                return (*this) << std::string(1, data);
            }

            // Read string data from the queue
            FileQueue& operator>>(std::string& data) {
                std::ifstream idx(filename_ + ".index");
                if (!idx) {
                    throw std::runtime_error("Failed to open index file for reading");
                }
                idx >> index_;
                idx.close();

                std::ifstream ifs(filename_ + ".txt", std::ios::binary);
                if (!ifs) {
                    throw std::runtime_error("Failed to open queue file for reading");
                }
                ifs.seekg(static_cast<std::streampos>(index_));
                if (!ifs) {
                    throw std::runtime_error("Failed to seek in queue file");
                }

                std::getline(ifs, data);
                if (ifs.eof() && data.empty()) {
                    data.clear(); // No more data to read
                    return *this;
                }
                if (!ifs && !ifs.eof()) {
                    throw std::runtime_error("Failed to read from queue file");
                }

                // Remove trailing \r if present (Windows line ending)
                if (!data.empty() && data.back() == '\r') {
                    data.pop_back();
                }

                auto pos = ifs.tellg();
                // Handle EOF case - tellg returns -1 at EOF
                if (pos == static_cast<std::streampos>(-1)) {
                    ifs.clear();
                    ifs.seekg(0, std::ios::end);
                    pos = ifs.tellg();
                }
                index_ = static_cast<double>(pos);
                
                std::ofstream idx_out(filename_ + ".index", std::ios::trunc);
                if (!idx_out) {
                    throw std::runtime_error("Failed to open index file for writing");
                }
                idx_out << index_;
                if (!idx_out) {
                    throw std::runtime_error("Failed to write to index file");
                }
                
                return *this;
            }
    };

    // For string messages in shared memory
    constexpr size_t MAX_STRING_SIZE = 256;

    struct QueueHeader {
        volatile size_t writeIndex;  // volatile = force memory read/write, no caching
        volatile size_t readIndex;   // works across processes (std::atomic doesn't!)
        size_t capacity;
        size_t itemSize;          // Size of each item in bytes
        size_t bufferSize;        // Total data buffer size
        bool isStringMode;        // true = string mode, false = binary/struct mode
    };

    template<typename T>
    class SHMQueue {
        private:
            HANDLE hMapFile_;       // Windows handle
            void* pMemory_;         // Pointer to shared memory
            QueueHeader* header_;   // Pointer to header in shared memory
            char* dataStart_;       // Pointer to data area in shared memory

            // Check if T is std::string
            static constexpr bool is_string_type() {
                return std::is_same<T, std::string>::value;
            }

            // Check if T is POD (Plain Old Data) - safe for memcpy
            static constexpr bool is_pod_type() {
                return std::is_trivially_copyable<T>::value && std::is_standard_layout<T>::value;
            }

        public:
            std::string name_;
            // Constructor - Creates or opens shared memory
            SHMQueue() : name_(), hMapFile_(NULL), pMemory_(nullptr), header_(nullptr), dataStart_(nullptr) {
                // Compile-time check: T must be either string or POD
                static_assert(is_string_type() || is_pod_type(), 
                    "T must be std::string or a POD type (no pointers, no std::string members, use char arrays instead)");
            }

            // FOR STRINGS: Uses default MAX_STRING_SIZE (256 bytes)
            int create_shm_queue(const std::string& name, const std::string& queue_type, size_t capacity){
                if (is_string_type()) {
                    return create_shm_queue(name, queue_type, capacity, MAX_STRING_SIZE);
                } else {
                    // For POD types, use sizeof(T)
                    return create_shm_queue(name, queue_type, capacity, sizeof(T));
                }
            }

            // FLEXIBLE VERSION: Custom size (for strings) or explicit size override
            int create_shm_queue(const std::string& name, const std::string& queue_type, size_t capacity, size_t itemSize){
                name_ = name;
                std::string fullName = name;

                // Determine actual item size
                size_t actualItemSize;
                bool isStringMode;
                
                if (is_string_type()) {
                    // String mode: use provided itemSize (or MAX_STRING_SIZE)
                    actualItemSize = itemSize;
                    isStringMode = true;
                } else {
                    // POD mode: use sizeof(T), ignore itemSize parameter
                    actualItemSize = sizeof(T);
                    isStringMode = false;
                }

                size_t totalSize = sizeof(QueueHeader) + (capacity * actualItemSize);

                hMapFile_ = CreateFileMappingA(
                    INVALID_HANDLE_VALUE,
                    NULL,
                    PAGE_READWRITE,
                    0,
                    totalSize,
                    fullName.c_str()
                );
                
                if (hMapFile_ == NULL) {
                    throw std::runtime_error("CreateFileMapping failed: " + std::to_string(GetLastError()));
                }
                
                pMemory_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, totalSize);
                
                if (pMemory_ == nullptr) {
                    CloseHandle(hMapFile_);
                    throw std::runtime_error("MapViewOfFile failed: " + std::to_string(GetLastError()));
                }

                // Initialize header
                header_ = static_cast<QueueHeader*>(pMemory_);
                header_->writeIndex = 0;
                header_->readIndex = 0;
                header_->capacity = capacity;
                header_->itemSize = actualItemSize;
                header_->bufferSize = capacity * actualItemSize;
                header_->isStringMode = isStringMode;
                
                dataStart_ = static_cast<char*>(pMemory_) + sizeof(QueueHeader);
                
                return 0;
            }
            int attach_to_shm_queue(const std::string& name,const std::string& queue_type){
                // function to attach to existing shared memory queue, can be used by consumer to connect to producer-created queue
                name_ = name;

                // For Windows, use the name directly (defaults to Local namespace)
                std::string fullName = name;

                // Open existing shared memory
                hMapFile_ = OpenFileMappingA(
                    FILE_MAP_ALL_ACCESS,
                    FALSE,
                    fullName.c_str()
                );
                
                if (hMapFile_ == NULL) {
                    throw std::runtime_error("OpenFileMapping failed: " + std::to_string(GetLastError()));
                }
                
                // Map into address space
                pMemory_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                
                if (pMemory_ == nullptr) {
                    CloseHandle(hMapFile_);
                    throw std::runtime_error("MapViewOfFile failed: " + std::to_string(GetLastError()));
                }

                // Set up pointers
                header_ = static_cast<QueueHeader*>(pMemory_);
                dataStart_ = static_cast<char*>(pMemory_) + sizeof(QueueHeader);
                
                return 0;
            }
            
            // Destructor - Cleanup
            ~SHMQueue() {
                if (pMemory_) UnmapViewOfFile(pMemory_);
                if (hMapFile_) CloseHandle(hMapFile_);
            }

            // Write to shared memory
            SHMQueue& operator<<(const T& data) {
                if (!header_) {
                    throw std::runtime_error("Queue not initialized!");
                }
                
                size_t writeIdx = header_->writeIndex;
                if (writeIdx >= header_->capacity) {
                    throw std::runtime_error("Queue full!");
                }
                
                char* slot = dataStart_ + (writeIdx * header_->itemSize);
                
                if constexpr (is_string_type()) {
                    // STRING MODE: Use strncpy (compile-time branch)
                    strncpy(slot, data.c_str(), header_->itemSize - 1);
                    slot[header_->itemSize - 1] = '\0';
                } else {
                    // POD MODE: Use memcpy for structs (compile-time branch)
                    memcpy(slot, &data, header_->itemSize);
                }
                
                header_->writeIndex = writeIdx + 1;
                
                return *this;
            }

            // Read from shared memory
            SHMQueue& operator>>(T& data) {
                if (!header_) {
                    throw std::runtime_error("Queue not initialized!");
                }
                
                size_t readIdx = header_->readIndex;
                size_t writeIdx = header_->writeIndex;
                
                if (readIdx >= writeIdx) {
                    throw std::runtime_error("Queue empty!");
                }
                
                char* slot = dataStart_ + (readIdx * header_->itemSize);
                
                if constexpr (is_string_type()) {
                    // STRING MODE: Construct string from null-terminated char array (compile-time branch)
                    data = std::string(slot);
                } else {
                    // POD MODE: Use memcpy for structs (compile-time branch)
                    memcpy(&data, slot, header_->itemSize);
                }
                
                header_->readIndex = readIdx + 1;
                
                return *this;
            }
    };

} // namespace shm_queue