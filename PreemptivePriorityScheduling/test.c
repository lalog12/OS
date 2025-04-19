#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Parse command line args: bursttime, priority, delay
    int burstTime = atoi(argv[1]);
    int priority = atoi(argv[2]); 
    int delayInS = atoi(argv[3]);
    
    // Ensure runtime is at least 10 seconds
    int runTime = (burstTime < 10) ? 10 : burstTime;
    
    // Print info and run for at least 10 seconds
    for (int i = 0; i < runTime; i++) {
        printf("Program %s: Priority %d, Burst time: %d, Time remaining: %d seconds, Delay: %d seconds\n", 
               argv[0], priority, burstTime, runTime - i, delayInS);
        sleep(1);
    }
    
    return 0;
}