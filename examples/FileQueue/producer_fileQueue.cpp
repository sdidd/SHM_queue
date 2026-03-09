#include <iostream>
#include <thread>
#include <chrono>
#include "shm_queue.hpp"

int main() {
    // Call FileQueue constructor to ensure files are created
    shm_queue::FileQueue<std::string> messages("messages");
    std::string msg;

    std::cout << "Producer starting..." << std::endl;
    std::cout << "Ready to send messages!" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
    
    // test messages to add to file
    std::string test_messages[] = {
        "Below are sensor readings:\n",
        "Temperature: 22.5°C\n",
        "Humidity: 45%\n",
        "Pressure: 1013 hPa\n",
        "End of sensor readings.\n"
    };
    while(true) {
        std::cout << "\nType your message:";
        std::getline(std::cin, msg); // Read message from user input
        messages << msg; // Write message to the queue
        std::cout << "\nMessage sent: " << msg << std::endl;
        std::this_thread::yield(); // Yield to allow consumer to read messages
    }
    
    return 0;
}