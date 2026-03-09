#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include "shm_queue.hpp"

int main() {
    std::cout << "Consumer starting..." << std::endl;
    
    // calling FileQueue constructor as consumer
    shm_queue::FileQueue<std::string> messages("messages");
    std::cout << "Ready to receive messages!" << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    while(true){
        std::string msg;
        messages >> msg; // Read message from the queue
        if (!msg.empty()) {
            std::cout << "Received message: " << msg << std::endl;
        } else {
            std::cout << "No new messages. Waiting..." << std::endl;
            std::this_thread::yield(); // Yield to allow producer to write messages
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Simulate delay
    }
    
    return 0;
}