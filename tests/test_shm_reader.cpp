#include <windows.h>
#include <iostream>

struct Message {
    char text[256];
};

int main() {
    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "Local\\MySharedMemory");
    
    if (hMapFile == NULL) {
        std::cerr << "Could not open file mapping object: " << GetLastError() << std::endl;
        return 1;
    }
    
    void* pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 1024);
    
    if (pBuf == NULL) {
        std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return 1;
    }
    
    Message* messages = static_cast<Message*>(pBuf);
    std::cout << "Read from shared memory: " << messages[0].text << std::endl;
    
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    
    return 0;
}