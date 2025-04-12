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
#include "schedule.h"


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

    const int FIRST_CMD_LINE_ARG = 2;
    
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

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 1000 * ms;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 1000 * ms;

    processSize = programNum;

    for(int i = 0; i < programNum; i++){
        if ((processIDs[i] = fork()) < 0){
            perror("fork");
            abort();
        }
        else if (processIDs[i] == 0){
            signal(SIGCONT, cont_handler);  
            char * cmdLine = programs[i];
            parseArgs(cmdLine, programArgs);
            pause();
            execv(programArgs[0], programArgs);
            freeArgs(programArgs);
            exit(1);
        }

        //printf("Child PID: %d\n", processIDs[i]);
    }


    setitimer(ITIMER_REAL, &timer, NULL);

    kill(processIDs[idx], SIGCONT);
    while (programNum > 0){
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

    const int FIRST_CMD_LINE_ARG = 2;
    // Checks for existence of program to be ran.
    if(argc <= FIRST_CMD_LINE_ARG){
        printf("Usage: %s ms_per_program [program args] : [...]\n", argv[0]);
        exit(1);
    }
    // Parses command line.
    for(int i = FIRST_CMD_LINE_ARG + 1; i < (argc); i++){
        // Checks for if program count goes over limit.
        if (programCount > MAX_PROCESSES){
            printf("Error: Number of programs (%d) exceed the the maximum allowed (%d).\n", programCount, MAX_PROCESSES);
            exit(1);
        }
        // Looking for new program to parse.
        if(*argv[i] == ':'){
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
            if(argumentCount > MAX_ARGUMENTS){
               // printf("Argument count for program (%s) is greater than %d\n", argv[i - argumentCount], MAX_ARGUMENTS);
                exit(1);
            }
        }
    }
   // printf("Past arguments: %d\n", argumentCount);
    //printf("Command line arguments are valid. Found %d programs, all within limits.\n", programCount);
}