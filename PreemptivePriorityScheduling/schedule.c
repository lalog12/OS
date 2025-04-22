#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include "schedule.h"
#include "linkedlist.h"


volatile int elapsedSeconds = 0;


int main(int argc, char * argv[]){
    validateCmdLine(argc, argv);
    roundRobinScheduler(argc, argv);
}

int populateArr(char * arr[], int argc, char * argv[]){
    
    int Index = 0;
    int stringLength = 0;
    int programCount = 0;

    const int FIRST_CMD_LINE_ARG = 1;
    
    for(int i = FIRST_CMD_LINE_ARG; i < (argc); i++){
        // skip command line argument.
        if(*argv[i] == ':'){
            stringLength = 0;
            Index += 1;
        }      
        // add program name to malloced array.
        else if(*argv[i - 1] == ':' || i == FIRST_CMD_LINE_ARG){
            stringLength = strlen(argv[i]) + 4;
            arr[Index] = malloc(stringLength);  // +4 "./", space, and null character.
            strcpy(arr[Index], "./");
            strcat(arr[Index], argv[i]);
            strcat(arr[Index], " ");
            programCount += 1;
        }

        else{ // add argument names to malloced array
            stringLength += strlen(argv[i]) + 1;
            char* temp = realloc(arr[Index], stringLength);
            if (temp == NULL){
                fprintf(stderr, "Failed to realloc\n");
                free(arr[Index]);
                exit(1);
            }
            arr[Index] = temp;
            strcat(arr[Index], argv[i]);
            strcat(arr[Index], " ");
        }
    }
    return programCount;
}

void timer_handler(int signum){
    elapsedSeconds++;
}

void cont_handler(int signum){
    
}

void executeRoundRobin(char *programs[], int programNum, char * argv[]){
    signal(SIGALRM, timer_handler);

    int ms = strtol(argv[1], NULL, 10); // quantum
    char * programArgs[MAX_ARGUMENTS + 2] = {NULL};  // +2 for executable and NULL
    // wait
    int status;

    int burstTime = 0;
    int priority = 0;
    int delay_in_s = 0;
    pid_t pid = 0;

    const int shortestJobFirst = 0;  // Enable Shortest Job First

    linkedList *inactiveList = createLinkedList();
    linkedList *activeList = createLinkedList();
    
    // Create all child processes, but keep them paused initially
    for(int i = 0; i < programNum; i++){
        char * cmdLine = programs[i];

        if ((pid = fork()) < 0){
            perror("fork");
            abort();
        }
        else if (pid == 0){ // child control flow
            signal(SIGCONT, cont_handler);  
            parseArgs(cmdLine, programArgs);
            pause(); // Wait until parent signals to continue
            execv(programArgs[0], programArgs);
            freeArgs(programArgs);
            exit(1);
        }
        else{ // parent control flow
            fill_var(&burstTime, &priority, &delay_in_s, &pid, cmdLine, inactiveList);
            // Use 0 for shortestJobFirst parameter when inserting to inactive list - always use delay ordering first
            insertLinkedList(burstTime, priority, delay_in_s, pid, inactiveList, 0, 0);  //inserting all processes into inactive linked list
        }
    }

    Node *poppedNode = NULL;
    pid_t currentRunningPID = -1; // Keep track of which process is currently running

    time_t start = time(NULL);
    printf("Starting scheduler at time %ld\n", start);
    // run while activeList or inactiveList has nodes
    while(inactiveList -> head != NULL ||  activeList -> head != NULL){  
        // Move processes from inactive to active based on their delay
        time_t current_time = time(NULL) - start;
        
        if (inactiveList->head != NULL) {
            printf("Current time: %ld, next process delay: %d\n", 
                   current_time, inactiveList->head->delay_in_s);
        }
        
        // Process the inactive list
        while(inactiveList->head != NULL && inactiveList->head->delay_in_s <= current_time){ 
            printf("Moving program %d from inactive list to active at time %ld\n", 
                   inactiveList->head->PID, current_time);
            
            // Debug active list before preemption decisions
            printActiveList(activeList, currentRunningPID);
            
            // Check if we have an active process and need to preempt based on SJF or priority
            if (activeList->head != NULL && currentRunningPID != -1) {
                // Check for preemption based on priority first
                if (inactiveList->head->priority > activeList->head->priority) {
                    printf("Preempting current process %d due to higher priority process %d (%d > %d)\n", 
                           currentRunningPID, inactiveList->head->PID,
                           inactiveList->head->priority, activeList->head->priority);
                    
                    // Pause current process
                    kill(currentRunningPID, SIGTSTP);
                    activeList->head->timeRan += time(NULL) - activeList->head->timeStarted;
                    
                    // Move new process to active list
                    poppedNode = popQueue(inactiveList);
                    insertLinkedList(poppedNode->burstTime, poppedNode->priority, 
                                    poppedNode->delay_in_s, poppedNode->PID, activeList, 1, shortestJobFirst);
                    
                    // Start the new higher priority process - PID is at active list head after insertion
                    currentRunningPID = activeList->head->PID;
                    kill(currentRunningPID, SIGCONT);
                    activeList->head->timeStarted = time(NULL);
                    
                    // Debug active list after preemption
                    printf("After priority preemption: ");
                    printActiveList(activeList, currentRunningPID);
                    
                    free(poppedNode);
                    continue;
                }
                
                // Only check for SJF preemption if shortestJobFirst is enabled
                else if (shortestJobFirst) {
                    // Calculate remaining time for current process
                    int currentRemaining = activeList->head->burstTime - activeList->head->timeRan;
                    // Calculate new process burst time
                    int newBurstTime = inactiveList->head->burstTime;
                    
                    printf("Current process %d has %d time remaining, new process %d has burst time %d\n",
                           currentRunningPID, currentRemaining, inactiveList->head->PID, newBurstTime);
                    
                    if (newBurstTime < currentRemaining) {
                        // New job is shorter, preempt current job
                        printf("Preempting current process %d for shorter job %d\n", 
                               currentRunningPID, inactiveList->head->PID);
                        
                        // Pause current process
                        kill(currentRunningPID, SIGTSTP);
                        activeList->head->timeRan += time(NULL) - activeList->head->timeStarted;
                        
                        // Move new process to active list
                        poppedNode = popQueue(inactiveList);
                        insertLinkedList(poppedNode->burstTime, poppedNode->priority, 
                                        poppedNode->delay_in_s, poppedNode->PID, activeList, 1, shortestJobFirst);
                        
                        // Start the new shorter job - PID is at active list head after insertion
                        currentRunningPID = activeList->head->PID;
                        kill(currentRunningPID, SIGCONT);
                        activeList->head->timeStarted = time(NULL);
                        
                        // Debug active list after preemption
                        printf("After SJF preemption: ");
                        printActiveList(activeList, currentRunningPID);
                        
                        free(poppedNode);
                        continue;
                    }
                }
                
                // No preemption needed, just add job to active list
                poppedNode = popQueue(inactiveList);
                insertLinkedList(poppedNode->burstTime, poppedNode->priority, 
                                poppedNode->delay_in_s, poppedNode->PID, activeList, 1, shortestJobFirst);
                
                // Debug active list after insertion (no preemption)
                printf("After insertion (no preemption): ");
                printActiveList(activeList, currentRunningPID);
                free(poppedNode);
            } else {
                // Move process to active list without preemption
                poppedNode = popQueue(inactiveList);
                insertLinkedList(poppedNode->burstTime, poppedNode->priority, 
                                poppedNode->delay_in_s, poppedNode->PID, activeList, 1, shortestJobFirst);
                
                // If no process is running yet, start this one
                if (currentRunningPID == -1) {
                    printf("Starting first process %d\n", activeList->head->PID);
                    currentRunningPID = activeList->head->PID;
                    kill(currentRunningPID, SIGCONT);
                    activeList->head->timeStarted = time(NULL);
                }
                
                // Debug active list after insertion (first process)
                printf("After first insertion: ");
                printActiveList(activeList, currentRunningPID);
                free(poppedNode);
            }
        }

        // Check active processes for completion
        if (activeList->head != NULL && currentRunningPID != -1) {
            // Verify that the running PID matches the head of the active list
            if (currentRunningPID != activeList->head->PID) {
                printf("ERROR: Running PID %d does not match active list head PID %d! Fixing...\n", 
                       currentRunningPID, activeList->head->PID);
                
                // Pause current process if it's still running
                kill(currentRunningPID, SIGTSTP);
                
                // Start the process at the head of the active list
                currentRunningPID = activeList->head->PID;
                kill(currentRunningPID, SIGCONT);
                activeList->head->timeStarted = time(NULL);
                
                printf("Fixed process state. Now running PID %d\n", currentRunningPID);
                printActiveList(activeList, currentRunningPID);
            }
            
            // Update running time for the active process - calculate time since last check
            time_t currentTime = time(NULL);
            time_t elapsedTime = currentTime - activeList->head->timeStarted;
            
            // Only update the tracking if time has actually passed
            if (elapsedTime > 0) {
                // Update total time ran
                activeList->head->timeRan += elapsedTime;
                
                // Update the start time to the current time for the next calculation
                activeList->head->timeStarted = currentTime;
            }
            
            // Check if the process has completed naturally
            int waitResult = waitpid(currentRunningPID, &status, WNOHANG);
            if (waitResult > 0) {
                // Process has terminated
                if (WIFEXITED(status)) {
                    printf("Process %d completed after running for %ld of %d seconds\n", 
                           currentRunningPID, activeList->head->timeRan, activeList->head->burstTime);
                } else {
                    printf("Process %d terminated abnormally after running for %ld seconds\n", 
                           currentRunningPID, activeList->head->timeRan);
                }
                
                // Remove from active list
                poppedNode = popQueue(activeList);
                free(poppedNode);
                
                // Reset current running PID
                currentRunningPID = -1;
                
                // Start next process if available
                if (activeList->head != NULL) {
                    printf("Starting next process %d\n", activeList->head->PID);
                    currentRunningPID = activeList->head->PID;
                    kill(currentRunningPID, SIGCONT);
                    activeList->head->timeStarted = time(NULL);
                    
                    // Debug active list after process completion
                    printf("After process completion: ");
                    printActiveList(activeList, currentRunningPID);
                }
            }
        }
        
        // If we have processes in the active list but none running, start the first one
        if (activeList->head != NULL && currentRunningPID == -1) {
            printf("Starting process %d from active list\n", activeList->head->PID);
            currentRunningPID = activeList->head->PID;
            kill(currentRunningPID, SIGCONT);
            activeList->head->timeStarted = time(NULL);
            
            // Debug active list after starting a new process
            printf("After starting new process: ");
            printActiveList(activeList, currentRunningPID);
        }
        
        // short sleep to prevent busy-waiting 
        // can quickly detect process completion
        usleep(5000); 
    }

    printf("End of while loop - all processes completed\n");
}


void roundRobinScheduler(int argc, char *argv[]){
    char *programs[MaxProcesses];
    int processes = populateArr(programs, argc, argv);
    executeRoundRobin(programs, processes, argv);

    for(int i = 0; i < processes; i++){
        if (programs[i] != NULL){
            free(programs[i]);
        }
    }
}

void fill_var(int * burstTime, int * priority, int * delay_in_s, int * pid, char * cmdline, linkedList *list){
    char *token;
    
    // Get the first token (program name) and discard it if needed
    token = strtok(cmdline, " ");
    
    // Get burst time
    token = strtok(NULL, " ");
    *burstTime = (int)strtol(token, NULL, 10);
    
    // Get priority
    token = strtok(NULL, " ");
    *priority = (int)strtol(token, NULL, 10);
    
    // Get delay
    token = strtok(NULL, " ");
    *delay_in_s = (int)strtol(token, NULL, 10);
}   

void removePID(int programNum, pid_t * PIDArr, pid_t rm){

    int idx = 0;

    while(PIDArr[idx] != rm){
        idx++;
    }

    while( (idx + 1) < programNum){
        PIDArr[idx] = PIDArr[idx + 1];
        idx++;
    }
}

void parseArgs(char * programs, char *programArgs[] ){
    char * programAndArgs = strtok(programs, " ");  // Program name

    programArgs[0] = strdup(programAndArgs);
    int idx = 1;

    while ((programAndArgs = strtok(NULL, " ")) != NULL){
        programArgs[idx] = strdup(programAndArgs);
        idx++;
    }
    programArgs[idx] = NULL;
}


void freeArgs(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
}

/* The validateCmdLine function parses the command line arguments and assesses whether
   the command line exceeds the amount of programs and arguments per program that are allowed. */
void validateCmdLine(int argc, char *argv[]){
    int programCount = 1;
    int argumentCount = 0;

    const int FIRST_PROG_AND_PARAMS = 1;

    // Checks for existence of program to be ran along with parameters of scheduler.
    if(argc < MIN_ARGS){
        printf("Usage: %s [prog1name] [bursttime] [priority] [delay_in_s] : [...]\n", argv[0]);
        exit(1);
    }

    // Parses command line.
    for(int i = FIRST_PROG_AND_PARAMS; i < (argc); i++){
        // Checks for if program count goes over limit.
        if (programCount > MAX_PROCESSES){
            printf("Error: Number of programs (%d) exceed the the maximum allowed (%d).\n", programCount, MAX_PROCESSES);
            exit(1);
        }
        // Looking for new program to parse.
        if(*argv[i] == ':' || i == FIRST_PROG_AND_PARAMS){
            //printf("Past arguments: %d\n", argumentCount);
            argumentCount = 0;
        }      
        // New program is found.
        else if(*argv[i - 1] == ':'){
            programCount += 1;
        }
        // Counting arguments in program. 
        else{
            argumentCount += 1;
            if(argumentCount > MAX_ARGUMENTS){ // +3 for burstTime, Priority, and delay_in_s.
                printf("Argument count for program (%s) is greater than %d\n", argv[i - argumentCount], MAX_ARGUMENTS);
                exit(1);
            }
        }

    }
   // printf("Past arguments: %d\n", argumentCount);
    //printf("Command line arguments are valid. Found %d programs, all within limits.\n", programCount);
}

// Add this function to debug active list state
void printActiveList(linkedList *list, pid_t currentRunningPID) {
    if (list->head == NULL) {
        printf("Active list is empty\n");
        return;
    }
    
    Node *cur = list->head;
    printf("Active list status (currentRunningPID = %d):\n", currentRunningPID);
    int i = 0;
    
    while (cur != NULL) {
        printf("  [%d] PID: %d, Priority: %d, Burst: %d, Remaining: %ld%s\n", 
               i++, cur->PID, cur->priority, cur->burstTime, 
               cur->burstTime - cur->timeRan,
               (cur->PID == currentRunningPID) ? " (RUNNING)" : "");
        cur = cur->next;
    }
    printf("\n");
}

