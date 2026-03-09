#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
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
    std::cout << "Consumer starting..." << std::endl;
    
    // calling SHMQueue constructor as consumer
    shm_queue::SHMQueue<SensorData> queue;

    try
    {
        queue.attach_to_shm_queue("sensor_queue", "local"); // Attach to the existing local shared memory queue created by producer
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    std::cout << "Ready to receive messages from " << queue.name_ << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    while(true){
        SensorData data;
        try {
            queue >> data;
            std::cout << "Message received: ID=" << data.id << ", Temp=" << data.temperature << "°C" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error reading message: " << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Simulate delay
    }
    
    return 0;
}