#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "linkedlist.h"

#define MAX_PROCESSES 200
#define MAX_ARGUMENTS 3
#define MIN_ARGS 5

int MaxProcesses = MAX_PROCESSES;
int MaxArguments = MAX_ARGUMENTS;

void validateCmdLine(int argc, char *argv[]);
int populateArr(char * arr[], int argc, char * argv[]);
void roundRobinScheduler(int argc, char *argv[]);
void executeRoundRobin(char *programs[], int programNum, char *argv[]);
void parseArgs(char * programs, char *programArgs[] );
void removePID(int programNum, pid_t * PIDArr, pid_t rm);
void timer_handler(int signum);
void freeArgs(char **args);
void fill_var(int * burstTime, int * priority, int * delay_in_s, int * pid, char * cmdline, linkedList *list);
void printActiveList(linkedList *list, pid_t currentRunningPID);

#endif