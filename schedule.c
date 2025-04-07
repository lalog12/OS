#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include "schedule.h"


int main(int argc, char * argv[]){
    validateCmdLine(argc, argv);
    roundRobinScheduler(argc, argv);

}

int populateArr(char * arr[], int argc, char * argv[]){
    
    int programsIndex = 0;
    int stringLength = 0;

    const int FIRST_CMD_LINE_ARG = 2;
    
    for(int i = FIRST_CMD_LINE_ARG; i < (argc); i++){
        // skip command line argument.
        if(*argv[i] == ':'){
            stringLength = 0;
            programsIndex += 1;
        }      
        // add program name to malloced array.
        else if(*argv[i - 1] == ':' || i == FIRST_CMD_LINE_ARG){
            stringLength = strlen(argv[i]) + 2;
            arr[programsIndex] = malloc(stringLength);  // +2 for space and null character.
            strcpy(arr[programsIndex], argv[i]);
            strcat(arr[programsIndex], " ");
        }

        else{
            stringLength += strlen(argv[i]) + 1;
            char* temp = realloc(arr[programsIndex], stringLength);
            if (temp == NULL){
                fprintf(stderr, "Failed to realloc\n");
                free(arr[programsIndex]);
                exit(1);
            }
            arr[programsIndex] = temp;
            strcat(arr[programsIndex], argv[i]);
            strcat(arr[programsIndex], " ");
        }
    }
    return programsIndex;
}



void roundRobinScheduler(int argc, char *argv[]){
    char *programs[MaxProcesses];
    int processes = populateArr(programs, argc, argv);

}


/* The validateCmdLine function parses the command line arguments and assesses whether
   the command line exceeds the amount of programs and arguments per program that are allowed. */
void validateCmdLine(int argc, char *argv[]){
    int programCount = 1;
    int argumentCount = 0;

    const int FIRST_CMD_LINE_ARG = 2;
    // Checks for existence of program to be ran.
    if(argc <= FIRST_CMD_LINE_ARG){
        printf("Usage: %s ms_per_program [program args] : [program args] : [...]\n", argv[0]);
        exit(1);
    }
    // Parses command line.
    for(int i = FIRST_CMD_LINE_ARG; i < (argc); i++){
        // Checks for if program count goes over limit.
        if (programCount > MAX_PROCESSES){
            printf("Error: Number of programs (%d) exceed the the maximum allowed (%d).\n", programCount, MAX_PROCESSES);
            exit(1);
        }
        // Looking for new program to parse.
        if(*argv[i] == ':'){
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
                printf("Argument count for program (%s) is greater than %d\n", argv[i - argumentCount + 1], MAX_ARGUMENTS);
                exit(1);
            }
        }
    }
    printf("Command line arguments are valid. Found %d programs, all within limits.\n", programCount);
}