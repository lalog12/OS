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


pid_t processIDs[MAX_PROCESSES]; // pid array
int processSize;
int idx;
pid_t currentProcess;

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
    
    if(processSize <= 0) return;
    kill(processIDs[idx], SIGSTOP);
    
    idx = (idx + 1) % processSize;

    kill(processIDs[idx], SIGCONT);

}

void cont_handler(int signum){
    
}

void executeRoundRobin(char *programs[], int programNum, char * argv[]){
    signal(SIGALRM, timer_handler);
    idx = 0;
    int ms = strtol(argv[1], NULL, 10); // quantum
    char * programArgs[MAX_ARGUMENTS + 2] = {NULL};  // +2 for executable and NULL
    // wait
    int status;


    int burstTime = 0;
    int priority = 0;
    int delay_in_s = 0;
    pid_t pid = 0;

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 1000 * ms;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000 * ms;

    processSize = programNum;

    linkedList *inactiveList = createLinkedList();
    linkedList *activeList = createLinkedList();

    for(int i = 0; i < programNum; i++){

        char * cmdLine = programs[i];

        if ((pid = fork()) < 0){
            perror("fork");
            abort();
        }
        else if (pid == 0){ // child control flow
            signal(SIGCONT, cont_handler);  
            parseArgs(cmdLine, programArgs);
            pause();
            execv(programArgs[0], programArgs);
            freeArgs(programArgs);
            exit(1);
        }
        else{ // parent control flow
            fill_var(&burstTime, &priority, &delay_in_s, &pid, cmdLine, inactiveList);
            insertLinkedList(burstTime, priority, delay_in_s, pid, inactiveList, 0);  //inserting all processes into inactive linked list
        }
    }

    Node *CurrentNode = popQueue(inactiveList);

    setitimer(ITIMER_REAL, &timer, NULL);

    kill(CurrentNode -> PID, SIGCONT);
    time_t start_time = time(NULL);  // start timer
    insertLinkedList(CurrentNode -> burstTime, CurrentNode -> priority, CurrentNode -> delay_in_s, CurrentNode -> PID, activeList, 1);

    while (inactiveList -> head != NULL ||  activeList -> head != NULL){
        pid_t pid = waitpid(-1, &status, WUNTRACED | WNOHANG);
        
        if(WIFEXITED(status) && pid > 0){
            // Check if this was the current process BEFORE removing it
            int was_current = (pid == processIDs[idx]);
            
            removePID(programNum, processIDs, pid);
            // printf("Waited for child: %d\n", pid);
            programNum--;
            processSize = programNum;
        
            // If there are no more processes, exit
            if(processSize <= 0) break;
        
            // If the exited process was the current one, start the next
            if(was_current){
                idx = idx % processSize;  
                kill(processIDs[idx], SIGCONT);
            }
        }
    }
}

// void fillLinkedList(char *programs[], linkedList * list, int programNum, int priorityFlag){
//     for(int i = 0; i < programNum; i++){
//         char *currentProgram = programs[i];
//         strtok(currentProgram, " ");
//         int burstTime = (int)strtol(strtok(currentProgram, " "), NULL, 10);
//         int priority = (int)strtol(strtok(currentProgram, " "), NULL, 10);
//         int delay_in_s = (int)strtol(strtok(currentProgram, " "), NULL, 10);
//         insertLinkedList(burstTime, priority, delay_in_s, list, 0);      // order by delay_in_s       
//     }

// }

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

