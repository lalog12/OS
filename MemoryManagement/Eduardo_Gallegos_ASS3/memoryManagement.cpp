#include "memoryManagement.h"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <iomanip>

MemoryManager::MemoryManager() : tlbIndex(0), nextBackingStoreFile(0), stopThreads(false), manualMode(false) {
    // Track program start time
    programStartTime = std::chrono::steady_clock::now();
    
    // Initialize TLB
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = false;
    }
    
    // Initialize frame allocation map and frameInsertionTimes vector
    frameInsertionTimes.resize(NUM_FRAMES);
    for (int i = 0; i < NUM_FRAMES; i++) {
        frameAllocation[i] = false;
        // Initialize with program start time
        frameInsertionTimes[i] = programStartTime;
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
        if (manualMode) {
            // In manual mode, only wait for and process explicit commands
            std::unique_lock<std::mutex> lock(processes[pid].commandMutex);
            
            // Wait indefinitely for a command or stop signal
            processes[pid].commandCV.wait(lock, [this, pid]() { 
                return (!processes[pid].commandQueue.empty() || stopThreads || !processes[pid].running); 
            });
            
            // If stopped or no longer running, exit
            if (stopThreads || !processes[pid].running) {
                break;
            }
            
            // Execute command
            if (!processes[pid].commandQueue.empty()) {
                ProcessCommand cmd = processes[pid].commandQueue.front();
                processes[pid].commandQueue.pop();
                
                // Reset the command completion flag before releasing lock
                processes[pid].commandCompleted = false;
                
                // Release lock before executing command
                lock.unlock();
                
                int result = 0;
                switch (cmd.type) {
                    case ProcessCommand::REQUEST_MEM: {
                        result = request_mem(pid, cmd.arg);
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
                        // Lock before modifying shared state
                        std::lock_guard<std::mutex> cmdLock(processes[pid].commandMutex);
                        processes[pid].commandCompleted = true;
                        processes[pid].commandCompletedCV.notify_all();
                        
                        end_process(pid);
                        return; // Exit thread immediately
                }
                
                // Signal that command has been completed
                {
                    std::lock_guard<std::mutex> cmdLock(processes[pid].commandMutex);
                    processes[pid].commandCompleted = true;
                    processes[pid].commandCompletedCV.notify_all(); // Use notify_all instead of notify_one
                }
            }
        }
        else {
            // RANDOM MODE BEHAVIOR:
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
    
    // Track frame insertion order (for age)
    static int insertionCounter = 0;
    frameInsertionTimes[frameNumber] = std::chrono::steady_clock::now();
    
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
        
        // Track frame insertion order (for age)
        static int insertionCounter = 0;
        frameInsertionTimes[frameNumber] = std::chrono::steady_clock::now();
        
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
        
        // Track frame insertion order (for age)
        static int insertionCounter = 0;
        frameInsertionTimes[frameNumber] = std::chrono::steady_clock::now();
        
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
    
    // Find the oldest frame for reference
    auto oldestTime = std::chrono::steady_clock::now();
    int oldestFrameIndex = -1;
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frameAllocation[i]) {
            if (oldestFrameIndex == -1 || frameInsertionTimes[i] < oldestTime) {
                oldestTime = frameInsertionTimes[i];
                oldestFrameIndex = i;
            }
        }
    }
    
    // Get current time for calculations
    auto currentTime = std::chrono::steady_clock::now();
    
    // Print header with formatted columns
    std::cout << "\n┌──────────────────────────────────────────────────────────────────┐\n";
    std::cout << "│                     PHYSICAL MEMORY STATUS                       │\n";
    std::cout << "├────────┬────────────┬───────────────────────────────┬────────────┤\n";
    std::cout << "│ Frame  │   Status   │         Association           │ Age (secs) │\n";
    std::cout << "├────────┼────────────┼───────────────────────────────┼────────────┤\n";
    
    // Print frame information
    for (int i = 0; i < NUM_FRAMES; i++) {
        std::cout << "│ " << std::setw(6) << std::left << i << " │ ";
        
        if (frameAllocation[i]) {
            std::cout << std::setw(10) << std::left << "Allocated" << " │ ";
            
            // Show process association if frame is allocated
            bool foundAssociation = false;
            for (size_t pid = 0; pid < processes.size(); pid++) {
                if (processes[pid].running) {
                    for (size_t pageNum = 0; pageNum < processes[pid].pageTable.size(); pageNum++) {
                        if (processes[pid].pageTable[pageNum].valid && 
                            processes[pid].pageTable[pageNum].inMemory &&
                            processes[pid].pageTable[pageNum].frameNumber == i) {
                            std::cout << std::setw(29) << std::left 
                                      << "Process: " + std::to_string(pid) + ", Page: " + std::to_string(pageNum) << " │ ";
                            foundAssociation = true;
                            break;
                        }
                    }
                    if (foundAssociation) break;
                }
            }
            
            if (!foundAssociation) {
                std::cout << std::setw(29) << std::left << "Not in page table" << " │ ";
            }
            
            // Age in seconds since allocation
            auto secondsAge = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - frameInsertionTimes[i]).count();
            
            std::string ageInfo;
            if (i == oldestFrameIndex) {
                ageInfo = std::to_string(secondsAge) + " (Oldest) ";
            } else {
                auto relativeAge = std::chrono::duration_cast<std::chrono::seconds>(
                    frameInsertionTimes[i] - frameInsertionTimes[oldestFrameIndex]).count();
                
                if (relativeAge < 0) {
                    ageInfo = std::to_string(secondsAge) + " (" + std::to_string(-relativeAge) + " newer)";
                } else if (relativeAge > 0) {
                    ageInfo = std::to_string(secondsAge) + " (" + std::to_string(relativeAge) + " older)";
                } else {
                    ageInfo = std::to_string(secondsAge) + " (Same)  ";
                }
            }
            
            std::cout << std::setw(10) << std::left << ageInfo << " │";
            
        } else {
            std::cout << std::setw(10) << std::left << "Free" << " │ "
                      << std::setw(29) << std::left << "N/A" << " │ "
                      << std::setw(10) << std::left << "N/A" << " │";
        }
        
        std::cout << "\n";
    }
    std::cout << "└────────┴────────────┴───────────────────────────────┴────────────┘\n";
    
    // Print TLB Status with better formatting
    std::cout << "\n┌──────────────────────────────────────┐\n";
    std::cout << "│             TLB STATUS               │\n";
    std::cout << "├────────┬────────────┬────────────────┤\n";
    std::cout << "│ Entry  │    Page    │     Frame      │\n";
    std::cout << "├────────┼────────────┼────────────────┤\n";
    
    bool tlbEntriesExist = false;
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].valid) {
            tlbEntriesExist = true;
            std::cout << "│ " << std::setw(6) << std::left << i << " │ "
                      << std::setw(10) << std::left << tlb[i].pageNumber << " │ "
                      << std::setw(14) << std::left << tlb[i].frameNumber << " │\n";
        }
    }
    
    if (!tlbEntriesExist) {
        std::cout << "│        No valid TLB entries            │\n";
    }
    
    std::cout << "└────────┴────────────┴────────────────┘\n";
    
    // Print backing store files and their associations with better formatting
    std::cout << "\n┌────────────────────────────────────────────┐\n";
    std::cout << "│           BACKING STORE FILES              │\n";
    std::cout << "├─────────┬─────────┬────────────────────────┤\n";
    std::cout << "│ Process │  Page   │       Filename         │\n";
    std::cout << "├─────────┼─────────┼────────────────────────┤\n";
    
    bool hasFiles = false;
    
    for (size_t pid = 0; pid < processes.size(); pid++) {
        if (processes[pid].running) {
            bool processHasFiles = false;
            
            for (size_t pageNum = 0; pageNum < processes[pid].pageTable.size(); pageNum++) {
                if (!processes[pid].pageTable[pageNum].backingStoreFile.empty()) {
                    hasFiles = true;
                    std::cout << "│ " << std::setw(7) << std::left << pid << " │ "
                              << std::setw(7) << std::left << pageNum << " │ "
                              << std::setw(24) << std::left 
                              << processes[pid].pageTable[pageNum].backingStoreFile << " │\n";
                }
            }
        }
    }
    
    if (!hasFiles) {
        std::cout << "│      No backing store files in use         │\n";
    }
    
    std::cout << "└─────────┴─────────┴────────────────────────┘\n";
    
    // Print file system information
    std::cout << "\n┌────────────────────────────────────────────┐\n";
    std::cout << "│           FILE SYSTEM USAGE                │\n";
    std::cout << "├──────────────────────────┬─────────────────┤\n";
    std::cout << "│ Backing Store Directory  │ " << std::setw(15) << std::left << backingStoreDir << " │\n";
    
    // Count number of files in backing store
    int fileCount = 0;
    size_t totalSize = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(backingStoreDir)) {
            if (std::filesystem::is_regular_file(entry)) {
                fileCount++;
                totalSize += std::filesystem::file_size(entry);
            }
        }
    } catch (std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing backing store: " << e.what() << std::endl;
    }
    
    std::cout << "│ Number of Files          │ " << std::setw(15) << std::left << fileCount << " │\n";
    std::cout << "│ Total Size               │ " << std::setw(12) << std::left << (totalSize / 1024.0) << " KB │\n";
    std::cout << "└──────────────────────────┴─────────────────┘\n";
}

void MemoryManager::handleCommand(const std::string& command) {
    std::unique_lock<std::mutex> lock(consoleMutex);
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    if (cmd == "Newprocess") {
        int mem;
        iss >> mem;
        
        if (iss.fail() || mem <= 0) {
            std::cout << "Error: Invalid memory size. Usage: Newprocess <memory_in_KB>" << std::endl;
            return;
        }
        
        // Convert KB to bytes
        int bytes = mem * 1024;
        int pid = init_mem(bytes);
        
        if (pid >= 0) {
            std::cout << "Created process " << pid << " with " << mem << "KB memory" << std::endl;
            createProcessThread(pid);
        } else {
            std::cout << "Failed to create process with " << mem << "KB memory" << std::endl;
        }
    }
    else if (cmd == "listprocess") {
        // Release the console mutex before calling listProcesses to avoid deadlock
        lock.unlock();
        listProcesses();
    }
    else if (cmd == "endprocess") {
        int pid;
        iss >> pid;
        
        if (iss.fail() || pid < 0) {
            std::cout << "Error: Invalid process ID. Usage: endprocess <pid>" << std::endl;
            return;
        }
        
        if (pid >= processes.size() || !processes[pid].running) {
            std::cout << "Error: Process " << pid << " does not exist or is no longer running" << std::endl;
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
        
        // Wait for the process to end in manual mode
        if (manualMode) {
            // For end_process, we wait until the process is no longer running or the command is completed
            std::unique_lock<std::mutex> cmdLock(processes[pid].commandMutex);
            bool success = processes[pid].commandCompletedCV.wait_for(cmdLock, 
                std::chrono::seconds(2),  // Add a timeout to prevent deadlock
                [this, pid]() {
                    return !processes[pid].running || processes[pid].commandCompleted || stopThreads;
                }
            );
            
            if (!success) {
                std::cout << "Warning: Timeout waiting for process " << pid << " to end" << std::endl;
                // Force end the process directly
                end_process(pid);
            }
        }
    }
    else if (cmd == "requestmem") {
        int pid, mem;
        iss >> pid >> mem;
        
        if (iss.fail() || pid < 0 || mem <= 0) {
            std::cout << "Error: Invalid parameters. Usage: requestmem <pid> <size_kb>" << std::endl;
            return;
        }
        
        if (pid >= processes.size() || !processes[pid].running) {
            std::cout << "Error: Process " << pid << " does not exist or is no longer running" << std::endl;
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
        
        // Wait for the command to complete in manual mode
        if (manualMode) {
            std::unique_lock<std::mutex> cmdLock(processes[pid].commandMutex);
            bool success = processes[pid].commandCompletedCV.wait_for(cmdLock, 
                std::chrono::seconds(2),  // Add a timeout to prevent deadlock
                [this, pid]() {
                    return processes[pid].commandCompleted || !processes[pid].running || stopThreads;
                }
            );
            
            if (!success) {
                std::cout << "Warning: Timeout waiting for memory request to complete" << std::endl;
            }
        }
    }
    else if (cmd == "accessmem") {
        int pid, addr;
        iss >> pid >> addr;
        
        if (iss.fail() || pid < 0 || pid >= processes.size() || !processes[pid].running || addr < 0) {
            std::cout << "Error: Invalid parameters. Usage: accessmem <pid> <address>" << std::endl;
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
        
        // Wait for the command to complete in manual mode
        if (manualMode) {
            std::unique_lock<std::mutex> cmdLock(processes[pid].commandMutex);
            bool success = processes[pid].commandCompletedCV.wait_for(cmdLock, 
                std::chrono::seconds(2),  // Add a timeout to prevent deadlock
                [this, pid]() {
                    return processes[pid].commandCompleted || !processes[pid].running || stopThreads;
                }
            );
            
            if (!success) {
                std::cout << "Warning: Timeout waiting for memory access to complete" << std::endl;
            }
        }
    }
    else if (cmd == "printmem") {
        // Release the console mutex before calling printMemory to avoid deadlock
        lock.unlock();
        printMemory();
    }
    else if (cmd == "help") {
        std::cout << "Available commands:" << std::endl;
        std::cout << "  Newprocess <size_kb> - Create a new process with specified memory in KB" << std::endl;
        std::cout << "  listprocess - List all active processes" << std::endl;
        std::cout << "  endprocess <pid> - Terminate the specified process" << std::endl;
        std::cout << "  requestmem <pid> <size_kb> - Request additional memory for a process" << std::endl;
        std::cout << "  accessmem <pid> <address> - Access memory at specified address for a process" << std::endl;
        std::cout << "  printmem - Display memory status" << std::endl;
        std::cout << "  end - Exit the program" << std::endl;
    }
    else if (cmd != "end") {
        std::cout << "Unknown command: " << cmd << ". Type 'help' for available commands." << std::endl;
    }
    
    // Ensure the output is flushed immediately
    std::cout << std::flush;
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
    // Make sure we have exclusive access to the console
    std::lock_guard<std::mutex> lock(consoleMutex);
    std::cout << "Stopping all processes...\n" << std::flush;
    stopThreads = true;
    cv.notify_all();
    
    // Notify all process threads to wake up
    for (auto& process : processes) {
        process.commandCV.notify_all();
        process.commandCompletedCV.notify_all();
    }
    
    // Release console lock before waiting for threads
    lock.~lock_guard(); // Explicitly destroy the lock to release mutex
    
    // Wait for all threads to complete with timeout
    {
        std::unique_lock<std::mutex> threadLock(threadMutex);
        bool allThreadsEnded = true;
        
        // Give threads 2 seconds to finish
        for (auto& pair : processThreads) {
            if (pair.second.joinable()) {
                // Use detach to avoid hanging if a thread doesn't respond
                pair.second.detach();
            }
        }
        processThreads.clear();
    }
    
    // Force terminate any remaining processes
    {
        std::lock_guard<std::mutex> finalLock(consoleMutex);
        
        // Cleanup all processes
        for (size_t i = 0; i < processes.size(); i++) {
            if (processes[i].running) {
                cleanupProcess(processes[i]);
                processes[i].running = false;
            }
        }
        
        std::cout << "All processes stopped.\n" << std::flush;
    }
}
