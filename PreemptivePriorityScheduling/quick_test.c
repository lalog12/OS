#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s [bursttime] [priority] [delay]\n", argv[0]);
        return 1;
    }
    
    int burstTime = atoi(argv[1]);
    int priority = atoi(argv[2]); 
    int delayInS = atoi(argv[3]);
    
    // printf("Program started: Burst=%d, Priority=%d, Delay=%d\n", burstTime, priority, delayInS);
    
    for (int i = 0; i < burstTime; i++) {
        printf("Running second %d of %d\n", i+1, burstTime);
        sleep(1);
    }
    
    printf("Program completed after %d seconds\n", burstTime);
    return 0;
} 