#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <sys/types.h>
#include <stddef.h>


typedef struct Node
{
    int burstTime;
    int priority;
    int delay_in_s;
    pid_t PID;
    struct Node * next;
    time_t timeRan;
    time_t timeStarted;
    time_t timeEnded;
}Node;


typedef struct linkedList
{
    Node *head;
}linkedList;

Node *popQueue(linkedList *list);
void insertLinkedList(int burstTime, int priority, int delay_in_s, int pid, linkedList * list, int priorityFlag, int shortestJobFirst);
linkedList *createLinkedList();

#endif   //endif