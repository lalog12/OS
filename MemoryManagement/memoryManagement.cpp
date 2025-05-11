#include "memoryManagement.h"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <filesystem>
#include <algorithm>

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
        // Sleep for one second
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Skip if process is no longer running
        if (!processes[pid].running) {
            break;
        }
        
        // Perform random action
        int action = actionDist(gen);
        
        if (action < 20) {
            // request_mem (20%)
            int mem = MIN_REQUEST_MEM + (gen() % 4) * PAGE_SIZE;
            request_mem(pid, mem);
            std::cout << "Process " << pid << " requested " << mem << " bytes of memory\n";
        }
        else if (action < 80) {
            // access_mem (60%)
            if (processes[pid].memorySize > 0) {
                int addr = gen() % processes[pid].memorySize;
                int value = access_mem(pid, addr);
                std::cout << "Process " << pid << " accessed address " << addr << ", value: " << value << "\n";
            }
        }
        else if (action < 90) {
            // end_process (10%)
            std::cout << "Process " << pid << " ending itself\n";
            end_process(pid);
            break; // Exit the thread
        }
        else {
            // start_new_process (10%)
            int mem = MIN_PROCESS_MEM + (gen() % 4) * PAGE_SIZE;
            int newPid = init_mem(mem);
            if (newPid >= 0) {
                std::cout << "Process " << pid << " started new process " << newPid << " with " << mem << " bytes\n";
                createProcessThread(newPid);
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
    if (frameNumber == -1) {
        // Need to implement page replacement algorithm
        // For now, just use the first frame
        frameNumber = 0;
    }
    
    // Update page table
    process.pageTable[pageNumber].frameNumber = frameNumber;
    process.pageTable[pageNumber].valid = true;
    process.pageTable[pageNumber].inMemory = true;
    
    // Update TLB
    updateTLB(pageNumber, frameNumber);
    
    // Load page from backing store if it exists
    if (!process.pageTable[pageNumber].backingStoreFile.empty()) {
        loadFromBackingStore(frameNumber, process.pageTable[pageNumber].backingStoreFile);
    }
}

void MemoryManager::saveToBackingStore(int frameNumber, const std::string& filename) {
    std::string filepath = backingStoreDir + "/" + filename;
    std::ofstream file(filepath, std::ios::binary);
    file.write(reinterpret_cast<char*>(physicalMemory[frameNumber]), PAGE_SIZE * sizeof(int));
}

void MemoryManager::loadFromBackingStore(int frameNumber, const std::string& filename) {
    std::string filepath = backingStoreDir + "/" + filename;
    std::ifstream file(filepath, std::ios::binary);
    file.read(reinterpret_cast<char*>(physicalMemory[frameNumber]), PAGE_SIZE * sizeof(int));
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
        int frameNumber = (i + 2) % NUM_FRAMES;
        // Make sure the frame is free
        if (frameAllocation[frameNumber]) {
            frameNumber = findFreeFrame();
        }
        
        if (frameNumber == -1) {
            return -1;  // No free frames available
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
        if (frameNumber == -1) {
            return -1;
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
    std::cout << "\nPhysical Memory Status:\n";
    for (int i = 0; i < NUM_FRAMES; i++) {
        std::cout << "Frame " << i << ": " 
                  << (frameAllocation[i] ? "Allocated" : "Free") << "\n";
    }
    
    std::cout << "\nTLB Status:\n";
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            std::cout << "Entry " << i << ": Page " << tlb[i].pageNumber 
                      << " -> Frame " << tlb[i].frameNumber << "\n";
        }
    }
}

void MemoryManager::handleCommand(const std::string& command) {
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
        end_process(pid);
    }
    else if (cmd == "requestmem") {
        int pid, mem;
        iss >> pid >> mem;
        request_mem(pid, mem * 1024);
    }
    else if (cmd == "accessmem") {
        int pid, addr;
        iss >> pid >> addr;
        
        // Calculate page number and offset
        int pageNumber = addr / PAGE_SIZE;
        int offset = addr % PAGE_SIZE;
        
        // Get frame number from TLB or page table
        int frameNumber = -1;
        
        // Check TLB first
        for (int i = 0; i < TLB_SIZE; i++) {
            if (tlb[i].valid && tlb[i].pageNumber == pageNumber) {
                frameNumber = tlb[i].frameNumber;
                std::cout << "TLB hit: ";
                break;
            }
        }
        
        // TLB miss - check page table
        if (frameNumber == -1) {
            if (pageNumber < processes[pid].pageTable.size() && 
                processes[pid].pageTable[pageNumber].valid) {
                
                frameNumber = processes[pid].pageTable[pageNumber].frameNumber;
                std::cout << "TLB miss, page table hit: ";
                
                if (!processes[pid].pageTable[pageNumber].inMemory) {
                    handlePageFault(processes[pid], pageNumber);
                    std::cout << "Page fault handled: ";
                }
            }
        }
        
        // Calculate physical address
        if (frameNumber != -1) {
            int physicalAddress = (frameNumber * PAGE_SIZE) + offset;
            int value = access_mem(pid, addr);
            
            std::cout << "Logical address " << addr 
                      << " (Page: " << pageNumber << ", Offset: " << offset << ")" 
                      << " → Physical address " << physicalAddress
                      << " (Frame: " << frameNumber << ", Offset: " << offset << ")"
                      << "\nValue at address: " << value << std::endl;
        } else {
            std::cout << "Invalid memory access: address " << addr 
                      << " not mapped for process " << pid << std::endl;
        }
    }
    else if (cmd == "printmem") {
        printMemory();
    }
}

void MemoryManager::startRandomProcessActivities() {
    // Create 5 initial processes with threads
    std::random_device rd;
    std::mt19937 gen(rd());
    
    for (int i = 0; i < 5; i++) {
        int mem = MIN_PROCESS_MEM + (gen() % 4) * PAGE_SIZE;
        int pid = init_mem(mem);
        if (pid >= 0) {
            std::cout << "Started initial process " << pid << " with " << mem << " bytes\n";
            createProcessThread(pid);
        }
    }
    
    // Run for exactly 10 seconds
    std::cout << "Process threads are running. Will stop after 10 seconds...\n";
    //printMemory();
    
    auto startTime = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::steady_clock::now() - startTime).count() < 10) {
        // Sleep for one second then print memory status
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "\n--- Memory status at " 
                  << std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::steady_clock::now() - startTime).count()
                  << " seconds ---\n";
        printMemory();
    }
    
    // Stop all threads
    stopRandomProcessActivities();
}

void MemoryManager::stopRandomProcessActivities() {
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