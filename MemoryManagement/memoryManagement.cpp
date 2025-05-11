#include "memoryManagement.h"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>

MemoryManager::MemoryManager() : tlbIndex(0), nextBackingStoreFile(0), stopThreads(false) {
    // Initialize TLB
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = false;
    }
    
    // Initialize frame allocation map
    for (int i = 0; i < NUM_FRAMES; i++) {
        frameAllocation[i] = false;
    }
    
    // Create backing store directory
    backingStoreDir = "backing_store";
    std::filesystem::create_directory(backingStoreDir);
}

MemoryManager::~MemoryManager() {
    // Stop all threads
    stopThreads = true;
    cv.notify_all();
    
    // Wait for threads to finish
    waitForAllThreads();
    
    // Cleanup backing store
    for (Process& process : processes) {
        cleanupProcess(process);
    }
    std::filesystem::remove_all(backingStoreDir);
}

// Function that runs in each thread to simulate a process
void MemoryManager::processThreadFunction(int pid) {
    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> actionDist(0, 99);
    
    while (!stopThreads && pid < processes.size() && processes[pid].running) {
        bool executed = false;
        
        // Check command queue first (with timeout)
        {
            std::unique_lock<std::mutex> lock(processes[pid].commandMutex);
            if (processes[pid].commandCV.wait_for(lock, std::chrono::seconds(1), 
                [this, pid]() { return !processes[pid].commandQueue.empty() || stopThreads; })) {
                // Execute command if available
                if (!processes[pid].commandQueue.empty() && processes[pid].running) {
                    ProcessCommand cmd = processes[pid].commandQueue.front();
                    processes[pid].commandQueue.pop();
                    
                    // Release lock before executing command
                    lock.unlock();
                    
                    switch (cmd.type) {
                        case ProcessCommand::REQUEST_MEM: {
                            int result = request_mem(pid, cmd.arg);
                            {
                                std::lock_guard<std::mutex> consoleLock(consoleMutex);
                                std::cout << "Process " << pid << " requested " << cmd.arg 
                                          << " bytes of memory, result: " 
                                          << (result == 0 ? "success" : "failure") << std::endl;
                            }
                            break;
                        }
                        case ProcessCommand::ACCESS_MEM: {
                            int value = access_mem(pid, cmd.arg);
                            {
                                std::lock_guard<std::mutex> consoleLock(consoleMutex);
                                std::cout << "Process " << pid << " accessed address " << cmd.arg
                                          << ", value: " << value << std::endl;
                            }
                            break;
                        }
                        case ProcessCommand::END_PROCESS:
                            {
                                std::lock_guard<std::mutex> consoleLock(consoleMutex);
                                std::cout << "Process " << pid << " ending itself" << std::endl;
                            }
                            end_process(pid);
                            return; // Exit thread
                    }
                    executed = true;
                }
            }
        }
        
        // In random mode, generate random action if no command was executed
        if (!executed && stopThreads == false && processes[pid].running) {
            // Generate random action
            int action = actionDist(gen);
            
            if (action < 20) {
                // request_mem (20%)
                int mem = MIN_REQUEST_MEM + (gen() % 4) * PAGE_SIZE;
                request_mem(pid, mem);
                // Synchronized console output
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Process " << pid << " requested " << mem << " bytes of memory\n";
            }
            else if (action < 80) {
                // access_mem (60%)
                if (processes[pid].memorySize > 0) {
                    int addr = gen() % processes[pid].memorySize;
                    int value = access_mem(pid, addr);
                    // Synchronized console output
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cout << "Process " << pid << " accessed address " << addr << ", value: " << value << "\n";
                }
            }
            else if (action < 90) {
                // end_process (10%)
                {
                    // Synchronized console output
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cout << "Process " << pid << " ending itself\n";
                }
                end_process(pid);
                break; // Exit the thread
            }
            else {
                // start_new_process (10%)
                int mem = MIN_PROCESS_MEM + (gen() % 4) * PAGE_SIZE;
                int newPid = init_mem(mem);
                if (newPid >= 0) {
                    {
                        // Synchronized console output
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "Process " << pid << " started new process " << newPid << " with " << mem << " bytes\n";
                    }
                    createProcessThread(newPid);
                }
            }
        }
    }
}

// Create a new thread for a process
void MemoryManager::createProcessThread(int pid) {
    std::lock_guard<std::mutex> lock(threadMutex);
    
    // Create a new thread for the process
    processThreads[pid] = std::thread(&MemoryManager::processThreadFunction, this, pid);
    processThreads[pid].detach(); // Detach to allow it to run independently
}

// Wait for all threads to finish
void MemoryManager::waitForAllThreads() {
    std::lock_guard<std::mutex> lock(threadMutex);
    
    // Join any remaining threads that are joinable
    for (auto& pair : processThreads) {
        if (pair.second.joinable()) {
            pair.second.join();
        }
    }
    
    processThreads.clear();
}

int MemoryManager::findFreeFrame() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frameAllocation[i]) {
            return i;
        }
    }
    return -1;  // No free frames
}

// Helper function to implement FIFO page replacement
int MemoryManager::replaceOldestFrame() {
    // Simple FIFO replacement - use the oldest frame
    static int victimFrame = 0;
    int frameNumber = victimFrame;
    victimFrame = (victimFrame + 1) % NUM_FRAMES;
    
    // Find which process/page is using this frame and save it to backing store
    for (size_t pid = 0; pid < processes.size(); pid++) {
        if (processes[pid].running) {
            for (size_t pgNum = 0; pgNum < processes[pid].pageTable.size(); pgNum++) {
                if (processes[pid].pageTable[pgNum].valid && 
                    processes[pid].pageTable[pgNum].inMemory &&
                    processes[pid].pageTable[pgNum].frameNumber == frameNumber) {
                    
                    // Create a backing store filename
                    std::string filename = "page_" + std::to_string(pid) + "_" + 
                                          std::to_string(pgNum) + "_" + 
                                          std::to_string(nextBackingStoreFile++);
                    
                    std::cout << "Swapping out: Process " << pid << ", Page " << pgNum 
                            << " from Frame " << frameNumber << " to " << filename << std::endl;
                    
                    // Save the current page to backing store
                    saveToBackingStore(frameNumber, filename);
                    
                    // Update the page table - page is valid but not in memory
                    processes[pid].pageTable[pgNum].inMemory = false;
                    processes[pid].pageTable[pgNum].backingStoreFile = filename;
                    
                    // Invalidate any TLB entries for this page
                    for (int i = 0; i < TLB_SIZE; i++) {
                        if (tlb[i].valid && tlb[i].frameNumber == frameNumber) {
                            tlb[i].valid = false;
                        }
                    }
                    
                    return frameNumber;
                }
            }
        }
    }
    
    // If we get here, the frame is allocated but not in any page table
    std::cout << "Warning: Frame " << frameNumber << " is marked as allocated but not found in any page table" << std::endl;
    return frameNumber;
}

void MemoryManager::updateTLB(int pageNumber, int frameNumber) {
    tlb[tlbIndex].pageNumber = pageNumber;
    tlb[tlbIndex].frameNumber = frameNumber;
    tlb[tlbIndex].valid = true;
    tlbIndex = (tlbIndex + 1) % TLB_SIZE;
}

void MemoryManager::handlePageFault(Process& process, int pageNumber) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    // Find a free frame
    int frameNumber = findFreeFrame();
    
    // If no free frames, implement page replacement
    if (frameNumber == -1) {
        frameNumber = replaceOldestFrame();
    }
    
    // Update page table
    process.pageTable[pageNumber].frameNumber = frameNumber;
    process.pageTable[pageNumber].valid = true;
    process.pageTable[pageNumber].inMemory = true;
    frameAllocation[frameNumber] = true;
    
    // Update TLB
    updateTLB(pageNumber, frameNumber);
    
    // Load page from backing store if it exists
    if (!process.pageTable[pageNumber].backingStoreFile.empty()) {
        std::cout << "Swapping in: Process " << process.pid << ", Page " << pageNumber 
                << " from " << process.pageTable[pageNumber].backingStoreFile 
                << " to Frame " << frameNumber << std::endl;
                
        loadFromBackingStore(frameNumber, process.pageTable[pageNumber].backingStoreFile);
        
        // Delete the backing store file
        std::filesystem::remove(backingStoreDir + "/" + process.pageTable[pageNumber].backingStoreFile);
        process.pageTable[pageNumber].backingStoreFile = "";
    }
}

void MemoryManager::saveToBackingStore(int frameNumber, const std::string& filename) {
    std::string filepath = backingStoreDir + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Error: Failed to open backing store file for writing: " << filepath << std::endl;
        return;
    }
    
    file.write(reinterpret_cast<char*>(physicalMemory[frameNumber]), PAGE_SIZE * sizeof(int));
    
    if (file.fail()) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Error: Failed to write data to backing store file: " << filepath << std::endl;
    }
    
    file.close();
}

void MemoryManager::loadFromBackingStore(int frameNumber, const std::string& filename) {
    std::string filepath = backingStoreDir + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open()) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Error: Failed to open backing store file for reading: " << filepath << std::endl;
        return;
    }
    
    file.read(reinterpret_cast<char*>(physicalMemory[frameNumber]), PAGE_SIZE * sizeof(int));
    
    if (file.fail()) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Error: Failed to read data from backing store file: " << filepath << std::endl;
        // Initialize the frame with zeros if read failed
        std::memset(physicalMemory[frameNumber], 0, PAGE_SIZE * sizeof(int));
    } else if (file.gcount() != PAGE_SIZE * sizeof(int)) {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Warning: Incomplete read from backing store file: " << filepath << std::endl;
        std::cerr << "Expected " << PAGE_SIZE * sizeof(int) << " bytes, got " << file.gcount() << " bytes" << std::endl;
    }
    
    file.close();
}

void MemoryManager::cleanupProcess(Process& process) {
    for (size_t i = 0; i < process.pageTable.size(); i++) {
        if (process.pageTable[i].valid) {
            frameAllocation[process.pageTable[i].frameNumber] = false;
            if (!process.pageTable[i].backingStoreFile.empty()) {
                std::filesystem::remove(backingStoreDir + "/" + process.pageTable[i].backingStoreFile);
            }
        }
    }
}

int MemoryManager::init_mem(int mem_requested) {
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (mem_requested < MIN_PROCESS_MEM) {
        return -1;  // Invalid memory request
    }
    
    // Create new process
    Process process(processes.size(), mem_requested);
    
    // Initialize page table
    int numPages = (mem_requested + PAGE_SIZE - 1) / PAGE_SIZE;
    process.pageTable.resize(numPages);
    
    // Allocate initial memory
    for (int i = 0; i < numPages; i++) {
        int frameNumber = findFreeFrame();
        // If no free frame, implement page replacement
        if (frameNumber == -1) {
            frameNumber = replaceOldestFrame();
        }
        
        process.pageTable[i].frameNumber = frameNumber;
        process.pageTable[i].valid = true;
        process.pageTable[i].inMemory = true;
        frameAllocation[frameNumber] = true;
        
        updateTLB(i, frameNumber);
    }
    
    processes.push_back(std::move(process));
    return processes.size() - 1; // Return the PID
}

int MemoryManager::request_mem(int pid, int mem_requested) {
    if (mem_requested < MIN_REQUEST_MEM) {
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(memoryMutex);
    
    if (pid >= processes.size() || !processes[pid].running) {
        return -1;
    }
    
    int numNewPages = (mem_requested + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t oldSize = processes[pid].pageTable.size();
    processes[pid].pageTable.resize(oldSize + numNewPages);
    processes[pid].memorySize += mem_requested;
    
    for (size_t i = oldSize; i < processes[pid].pageTable.size(); i++) {
        int frameNumber = findFreeFrame();
        // If no free frame, implement page replacement
        if (frameNumber == -1) {
            frameNumber = replaceOldestFrame();
        }
        
        processes[pid].pageTable[i].frameNumber = frameNumber;
        processes[pid].pageTable[i].valid = true;
        processes[pid].pageTable[i].inMemory = true;
        frameAllocation[frameNumber] = true;
        
        updateTLB(i, frameNumber);
    }
    
    return 0;
}

int MemoryManager::access_mem(int pid, int address) {
    if (pid >= processes.size() || !processes[pid].running) {
        return -1;
    }
    
    int pageNumber = address / PAGE_SIZE;
    int offset = address % PAGE_SIZE;
    
    // Check TLB first
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid && tlb[i].pageNumber == pageNumber) {
            return physicalMemory[tlb[i].frameNumber][offset];
        }
    }
    
    // TLB miss - check page table
    if (pageNumber >= processes[pid].pageTable.size() || !processes[pid].pageTable[pageNumber].valid) {
        return -1;
    }
    
    if (!processes[pid].pageTable[pageNumber].inMemory) {
        handlePageFault(processes[pid], pageNumber);
    }
    
    return physicalMemory[processes[pid].pageTable[pageNumber].frameNumber][offset];
}

void MemoryManager::end_process(int pid) {
    if (pid >= processes.size()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(memoryMutex);
    cleanupProcess(processes[pid]);
    processes[pid].running = false;
}

void MemoryManager::start_new_process(int mem_requested) {
    int pid = init_mem(mem_requested);
    if (pid >= 0) {
        createProcessThread(pid);
    }
}

void MemoryManager::listProcesses() {
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << "\nActive Processes:\n";
    for (const auto& process : processes) {
        if (process.running) {
            std::cout << "PID: " << process.pid 
                      << ", Memory: " << process.memorySize 
                      << " bytes, Pages: " << process.pageTable.size() << "\n";
        }
    }
}

void MemoryManager::printMemory() {
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << "\nPhysical Memory Status:\n";
    for (int i = 0; i < NUM_FRAMES; i++) {
        std::cout << "Frame " << i << ": " 
                  << (frameAllocation[i] ? "Allocated" : "Free");
        
        // Show process association if frame is allocated
        if (frameAllocation[i]) {
            bool foundAssociation = false;
            for (size_t pid = 0; pid < processes.size(); pid++) {
                if (processes[pid].running) {
                    for (size_t pageNum = 0; pageNum < processes[pid].pageTable.size(); pageNum++) {
                        // std::cout << "valid" << ": " << processes[pid].pageTable[pageNum].valid << std::endl;
                        // std::cout << "inMemory" << ": " << processes[pid].pageTable[pageNum].inMemory << std::endl;
                        // std::cout << "frameNumber" << ": " << processes[pid].pageTable[pageNum].frameNumber << std::endl;
                        // std::cout << "i" << ": " << i << std::endl;
                        if (processes[pid].pageTable[pageNum].valid && 
                            processes[pid].pageTable[pageNum].inMemory &&
                            processes[pid].pageTable[pageNum].frameNumber == i) {
                            std::cout << " (Process: " << pid << ", Page: " << pageNum << ")";
                            foundAssociation = true;
                            break;
                        }
                    }
                    if (foundAssociation) {
                        break;
                    }
                }
            }
            if (!foundAssociation) {
                std::cout << " (Not in page table)" << std::endl;
            }
        }
        
        std::cout << "\n";
    }
    
    std::cout << "\nTLB Status:\n";
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            std::cout << "Entry " << i << ": Page " << tlb[i].pageNumber 
                      << " -> Frame " << tlb[i].frameNumber << "\n";
        }
    }
    
    // Print backing store files and their associations
    std::cout << "\nBacking Store Files:\n";
    bool hasFiles = false;
    
    for (size_t pid = 0; pid < processes.size(); pid++) {
        if (processes[pid].running) {
            bool processHasFiles = false;
            
            for (size_t pageNum = 0; pageNum < processes[pid].pageTable.size(); pageNum++) {
                if (!processes[pid].pageTable[pageNum].backingStoreFile.empty()) {
                    if (!processHasFiles) {
                        std::cout << "Process " << pid << ":\n";
                        processHasFiles = true;
                    }
                    std::cout << "  Page " << pageNum << " -> File: " 
                              << processes[pid].pageTable[pageNum].backingStoreFile << "\n";
                    hasFiles = true;
                }
            }
        }
    }
    
    if (!hasFiles) {
        std::cout << "No backing store files in use.\n";
    }
}

void MemoryManager::handleCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "Newprocess") {
        int mem;
        iss >> mem;
        start_new_process(mem * 1024);  // Convert KB to bytes
    }
    else if (cmd == "listprocess") {
        listProcesses();
    }
    else if (cmd == "endprocess") {
        int pid;
        iss >> pid;
        
        if (pid >= processes.size() || !processes[pid].running) {
            std::cout << "Error: Invalid process ID " << pid << std::endl;
            return;
        }
        
        // Queue end process command to the process's thread
        {
            std::lock_guard<std::mutex> cmdLock(processes[pid].commandMutex);
            ProcessCommand command;
            command.type = ProcessCommand::END_PROCESS;
            command.arg = 0;
            processes[pid].commandQueue.push(command);
        }
        processes[pid].commandCV.notify_one();
        
        std::cout << "Sent end command to process " << pid << std::endl;
    }
    else if (cmd == "requestmem") {
        int pid, mem;
        iss >> pid >> mem;
        
        if (pid >= processes.size() || !processes[pid].running) {
            std::cout << "Error: Invalid process ID " << pid << std::endl;
            return;
        }
        
        // Queue request mem command to the process's thread
        {
            std::lock_guard<std::mutex> cmdLock(processes[pid].commandMutex);
            ProcessCommand command;
            command.type = ProcessCommand::REQUEST_MEM;
            command.arg = mem * 1024;  // Convert KB to bytes
            processes[pid].commandQueue.push(command);
        }
        processes[pid].commandCV.notify_one();
        
        std::cout << "Sent memory request command to process " << pid << std::endl;
    }
    else if (cmd == "accessmem") {
        int pid, addr;
        iss >> pid >> addr;
        
        if (pid >= processes.size() || !processes[pid].running) {
            std::cout << "Error: Invalid process ID " << pid << std::endl;
            return;
        }
        
        // Queue access mem command to the process's thread
        {
            std::lock_guard<std::mutex> cmdLock(processes[pid].commandMutex);
            ProcessCommand command;
            command.type = ProcessCommand::ACCESS_MEM;
            command.arg = addr;
            processes[pid].commandQueue.push(command);
        }
        processes[pid].commandCV.notify_one();
        
        std::cout << "Sent memory access command to process " << pid << std::endl;
    }
    else if (cmd == "printmem") {
        printMemory();
    }
}

void MemoryManager::startRandomProcessActivities() {
    // Create 5 initial processes with threads
    std::random_device rd;
    std::mt19937 gen(rd());
    
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Creating 5 initial processes...\n";
    }
    
    for (int i = 0; i < 5; i++) {
        int mem = MIN_PROCESS_MEM + (gen() % 4) * PAGE_SIZE;
        int pid = init_mem(mem);
        if (pid >= 0) {
            // Synchronized console output
            {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "Started initial process " << pid << " with " << mem << " bytes\n";
            }
            createProcessThread(pid);
        }
    }
    
    // Run for exactly 10 seconds
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cout << "Process threads are running. Will stop after 10 seconds...\n";
    }
    printMemory();
    
    
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - startTime).count() < 10) {
        // Sleep for one second then print memory status
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "\n--- Memory status at " 
                    << std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - startTime).count()
                    << " seconds ---\n";
        }
        printMemory();
    }
    
    // Stop all threads
    stopRandomProcessActivities();
}

void MemoryManager::stopRandomProcessActivities() {
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << "Stopping all processes...\n";
    stopThreads = true;
    cv.notify_all();
    waitForAllThreads();
    
    // Cleanup all processes
    for (size_t i = 0; i < processes.size(); i++) {
        if (processes[i].running) {
            end_process(i);
        }
    }
}