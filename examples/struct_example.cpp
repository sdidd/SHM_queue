#include <iostream>
#include "shm_queue.hpp"

// Example POD struct - ALL members must be fixed-size (no std::string!)
struct SensorData {
    double id;
    double temperature;
    double humidity;
    char location[50];  // Fixed-size char array, NOT std::string!
    int timestamp;
};

int main() {
    // Create queue for struct
    shm_queue::SHMQueue<SensorData> queue;
    queue.create_shm_queue("sensor_queue", "local", 5);  // Capacity: 5 structs
    
    std::cout << "Size of SensorData: " << sizeof(SensorData) << " bytes" << std::endl;
    std::cout << "Writing sensor data to shared memory..." << std::endl;
    
    // Write some sensor data
    SensorData data1 = {1.0, 22.5, 45.0, "Room A", 1234567890};
    queue << data1;
    std::cout << "Wrote: ID=" << data1.id << ", Temp=" << data1.temperature << "°C" << std::endl;
    
    SensorData data2 = {2.0, 23.1, 47.5, "Room B", 1234567900};
    queue << data2;
    std::cout << "Wrote: ID=" << data2.id << ", Temp=" << data2.temperature << "°C" << std::endl;
    
    // Read it back
    SensorData readData;
    queue >> readData;
    std::cout << "\nRead back: ID=" << readData.id 
              << ", Temp=" << readData.temperature << "°C"
              << ", Location=" << readData.location << std::endl;
    
    return 0;
}
