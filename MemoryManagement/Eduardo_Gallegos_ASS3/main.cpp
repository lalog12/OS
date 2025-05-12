#include "memoryManagement.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    MemoryManager mm;
    bool randomMode = true;
    
    if (argc > 1 && std::string(argv[1]) == "--manual") {
        randomMode = false;
        mm.setManualMode(true);
    }
    
    if (randomMode) {
        std::cout << "Starting in random mode with 5 threads...\n";
        mm.startRandomProcessActivities();
    }
    else {
        // Manual mode
        std::string command;
        bool exit_requested = false;
        
        std::cout << "Starting in manual mode. Type 'end' to exit.\n";
        while (!exit_requested) {
            std::cout << "> " << std::flush;
            std::getline(std::cin, command);
            
            if (command == "end") {
                std::cout << "Exiting program...\n" << std::flush;
                exit_requested = true;
            }
            else {
                mm.handleCommand(command);
            }
        }
        
        // Clean up resources when exiting
        mm.stopRandomProcessActivities(); 
    }
    
    // Ensure destructor is called
    return 0;
} 