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


    Node *poppedNode = NULL;

    time_t start = time(NULL);
    // run while activeList or inactiveList has nodes
    while(inactiveList -> head != NULL ||  activeList -> head != NULL){  
        // printf("Inside while loop that runs when activeList || inactiveList\n");
        // move all programs from inactive list into activeList that are scheduled to start at elapsedSeconds.
        while(inactiveList->head != NULL &&
        inactiveList -> head -> delay_in_s <= (time(NULL) - start)){ 
            printf("Moving programs from inactive list to active at time %ld\n", time(NULL)-start);
            // move process to active linked list with preemption
            if ( (inactiveList->head != NULL && activeList->head != NULL) &&
                (inactiveList -> head -> priority) > (activeList -> head -> priority) ){   
                printf("Moving process to active List with preemption\n");
                // Pause current process
                kill(activeList -> head -> PID, SIGTSTP);
                activeList -> head -> timeRan += time(NULL) - activeList -> head -> timeStarted;

                poppedNode = popQueue(inactiveList);
                insertLinkedList(poppedNode ->burstTime, poppedNode -> priority, poppedNode ->delay_in_s, poppedNode ->PID, activeList, 1);

                kill(activeList -> head -> PID, SIGCONT);
                activeList -> head -> timeStarted = time(NULL);
                
                free(poppedNode);
            }   

            else{   // move process to active linked list with no preemption.
                printf("Move process to active list with no preemption\n");
                poppedNode = popQueue(inactiveList);
                insertLinkedList(poppedNode -> burstTime, poppedNode -> priority, poppedNode -> delay_in_s, poppedNode -> PID, activeList, 1);

                // moved process is only process in activeList
                if(activeList -> head -> PID == poppedNode -> PID){
                    printf("Current process is only process in active list\n");
                    kill(activeList -> head -> PID, SIGCONT);
                    activeList -> head -> timeStarted = time(NULL);
                }
                printf("Freeing node that was in inactive list\n");
                free(poppedNode);
            }    
        }

        if (activeList -> head != NULL){
            // printf("While loop for when the activeList head != NULL\n");
            // Program being ran completed successfully.
            activeList -> head -> timeRan +=  (time(NULL) - activeList -> head -> timeStarted);
            activeList -> head -> timeStarted = time(NULL);
            



            if( (activeList -> head -> timeRan >= activeList -> head ->burstTime) && (WIFEXITED(status)) ){ 
                printf("Inside while loop for when timeRan exceeds burstTime and WIFEXITED(status) returns True\n");
                int waitResult = waitpid(activeList -> head -> PID, &status, WNOHANG | WUNTRACED);
                if (waitResult > 0 && WIFEXITED(status)){
                    printf("process %d completed successfully\n", activeList -> head -> PID);
                }
                else{
                    printf("Inside while loop for when timeRan exceeds burstTime and WIFEXITED(status) returns True and freeing node");
                    poppedNode = popQueue(activeList);
                    free(poppedNode);

                    // Start new program if allowed.
                    if(activeList -> head != NULL){
                        printf("Starting new program\n");
                        kill(activeList -> head -> PID, SIGCONT);
                        activeList -> head -> timeStarted = time(NULL);
                    }
                }

            }
        }


    }

    printf("End of while loop\n");
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

