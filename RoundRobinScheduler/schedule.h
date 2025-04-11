#ifndef SCHEDULE_H
#define SCHEDULE_H

#define MAX_PROCESSES 200
#define MAX_ARGUMENTS 10

int MaxProcesses = MAX_PROCESSES;
int MaxArguments = MAX_ARGUMENTS;

void validateCmdLine(int argc, char *argv[]);
int populateArr(char * arr[], int argc, char * argv[]);
void roundRobinScheduler(int argc, char *argv[]);
void executeRoundRobin(char *programs[], int programNum);
void parseArgs(char * programs, char *programArgs[] );

#endif