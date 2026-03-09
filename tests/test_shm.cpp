#include<iostream>
#include<windows.h>

int main() {
    std::cout << "Testing shared memory queue..." << std::endl;

    // Create a file mapping object for shared memory
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // Use paging file
        NULL,                    // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Maximum object size (high-order DWORD)
        1024,                    // Maximum object size (low-order DWORD)      // Name of the mapping object
        "Local\\MySharedMemory");      // Name of the mapping object

    if (hMapFile == NULL) {
        std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
        return 1;
    }

    // Map the view of the file into the address space of the process
    LPCTSTR pBuf = (LPTSTR) MapViewOfFile(hMapFile,   // Handle to map object
                                          FILE_MAP_ALL_ACCESS, // Read/write permission
                                          0,
                                          0,
                                          1024);

    if (pBuf == NULL) {
        std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }

    // Write a test message to shared memory
    char* message = "Hello from shared memory!";
    char* pMessage = "Shut up and take my money!";
    try
    {
        CopyMemory((PVOID)pBuf, message, strlen(message) + 1);
        CopyMemory((PVOID)pBuf, pMessage, strlen(pMessage) + 1);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    std::cout << "Message written to shared memory: " << message << " and " << pMessage << std::endl;

    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    // Clean up
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);

    return 0;
}