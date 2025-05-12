#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include <vector>
#include <queue>
#include <map>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <fstream>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <chrono>

// Constants
const int PAGE_SIZE = 4096;  // 4KB
const int NUM_FRAMES = 20;   // 20 frames of 4KB each
const int TLB_SIZE = 5;      // 5 entry TLB
const int MIN_PROCESS_MEM = 8192;  // 8KB minimum for process
const int MIN_REQUEST_MEM = 4096;  // 4KB minimum for memory request

// Command structure for process threads
struct ProcessCommand {
    enum Type {
        REQUEST_MEM,
        ACCESS_MEM,
        END_PROCESS
    };
    
    Type type;
    int arg;  // Either mem_requested or address depending on type
};

// TLB Entry structure
struct TLBEntry {
    int pageNumber;
    int frameNumber;
    bool valid;
};

// Page Table Entry structure
struct PageTableEntry {
    int frameNumber;
    bool valid;
    bool inMemory;
    std::string backingStoreFile;  // File name in backing store
};

// Process structure
class Process {
public:
    int pid;
    std::vector<PageTableEntry> pageTable;
    int memorySize;
    std::atomic<bool> running;
    
    // Command queue for this process
    std::queue<ProcessCommand> commandQueue;
    std::mutex commandMutex;
    std::condition_variable commandCV;
    
    // For command completion synchronization
    bool commandCompleted;
    std::condition_variable commandCompletedCV;

    Process() : pid(0), memorySize(0), running(true), commandCompleted(false) {}
    Process(int p, int mem) : pid(p), memorySize(mem), running(true), commandCompleted(false) {}
    
    // Delete copy constructor and assignment
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;
    
    // Implement move constructor and assignment
    Process(Process&& other) noexcept
        : pid(other.pid)
        , pageTable(std::move(other.pageTable))
        , memorySize(other.memorySize)
        , running(other.running.load())
        , commandCompleted(other.commandCompleted) {
        other.running = false;
    }
    
    Process& operator=(Process&& other) noexcept {
        if (this != &other) {
            pid = other.pid;
            pageTable = std::move(other.pageTable);
            memorySize = other.memorySize;
            running = other.running.load();
            commandCompleted = other.commandCompleted;
            other.running = false;
        }
        return *this;
    }
};

class MemoryManager {
private:
    // Physical memory (20 frames of 4KB each)
    int physicalMemory[NUM_FRAMES][PAGE_SIZE];
    
    // Frame age tracking using timestamps (in seconds since program start)
    std::vector<std::chrono::time_point<std::chrono::steady_clock>> frameInsertionTimes;
    
    // TLB
    TLBEntry tlb[TLB_SIZE];
    int tlbIndex;
    
    // Process management
    std::vector<Process> processes;
    std::map<int, bool> frameAllocation;  // Tracks which frames are allocated
    std::mutex memoryMutex;
    
    // Thread management
    std::mutex threadMutex;
    std::mutex consoleMutex;  // Mutex for console output
    std::condition_variable cv;
    std::atomic<bool> stopThreads;
    std::unordered_map<int, std::thread> processThreads;
    
    // Backing store
    std::string backingStoreDir;
    int nextBackingStoreFile;
    
    // Program start time for age calculations
    std::chrono::time_point<std::chrono::steady_clock> programStartTime;
    
    // Mode flags
    bool manualMode;
    
    // Helper functions
    int findFreeFrame();
    int replaceOldestFrame();
    void updateTLB(int pageNumber, int frameNumber);
    void handlePageFault(Process& process, int pageNumber);
    void saveToBackingStore(int frameNumber, const std::string& filename);
    void loadFromBackingStore(int frameNumber, const std::string& filename);
    void cleanupProcess(Process& process);
    
    // Thread function that simulates a process
    void processThreadFunction(int pid);

public:
    MemoryManager();
    ~MemoryManager();
    
    // Process management
    int init_mem(int mem_requested);
    int request_mem(int pid, int mem_requested);
    int access_mem(int pid, int address);
    void end_process(int pid);
    void start_new_process(int mem_requested);
    
    // Thread management
    void createProcessThread(int pid);
    void waitForAllThreads();
    
    // Command line interface
    void handleCommand(const std::string& command);
    void listProcesses();
    void printMemory();
    
    // Random process activities
    void startRandomProcessActivities();
    void stopRandomProcessActivities();

    // For testing
    size_t getProcessCount() const { return processes.size(); }
    int getProcessMemorySize(int pid) const { 
        return (pid < processes.size()) ? processes[pid].memorySize : 0; 
    }
    
    // Set the operation mode
    void setManualMode(bool manual) { manualMode = manual; }
};

#endif // MEMORY_MANAGEMENT_H