#include "memoryManagement.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    MemoryManager mm;
    bool randomMode = true;
    
    if (argc > 1 && std::string(argv[1]) == "--manual") {
        randomMode = false;
    }
    
    if (randomMode) {
        std::cout << "Starting in random mode with 5 threads...\n";
        mm.startRandomProcessActivities();
    }
    else {
        // Manual mode
        std::string command;
        std::cout << "Starting in manual mode. Type 'end' to exit.\n";
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, command);
            
            if (command == "end") {
                break;
            }
            
            mm.handleCommand(command);
        }
    }
    
    return 0;
} 