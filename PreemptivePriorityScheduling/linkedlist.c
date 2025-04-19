#include <stdio.h>
#include <stdlib.h>

#include "linkedlist.h"





// int main(int argc, char * argv[]){
//     linkedList * list = createLinkedList();
//     insertLinkedList(2, 1, 1, 0, list, 1);
//     insertLinkedList(1, 5, 1, 0, list, 1);
//     insertLinkedList(4, 6, 9, 0, list, 1);
//     insertLinkedList(5, 6, 6, 0, list, 1);
//     insertLinkedList(9, 6, 2, 0, list, 1);

//     Node * cur = list -> head;
//     while (cur != NULL){
//         printf("priority: %d\n", cur -> priority);
//         printf("delay_in_s: %d\n", cur -> delay_in_s);
//         cur = cur -> next;
//     }
// }

void insertLinkedList(int burstTime, int priority, int delay_in_s, int pid, linkedList * list, int priorityFlag){
    Node * new_node = (Node *)malloc(sizeof(Node));
    if (new_node == NULL){
        printf("Malloc failed");
        exit(1);
    }

    new_node->burstTime = burstTime;    // populate with info
    new_node->priority = priority;
    new_node->delay_in_s = delay_in_s; 
    new_node-> PID = pid;
    new_node -> next = NULL;


    if (list->head == NULL){    // insert into empty list
        list->head = new_node;
        new_node->next = NULL;
        return;
    }

    if (priorityFlag == 1){     // priority ordering
        printf("ordering with priority\n");
        if (new_node -> priority > list -> head -> priority){  // inserting at the head
            new_node -> next = list -> head;
            list -> head = new_node;
            return;
        }

        Node *cur = list -> head;  // start at head of list
        
        while (cur != NULL){
    
            if (cur -> next == NULL){ // inserting in tail of linked list
                cur -> next = new_node;
                new_node -> next = NULL;
                return;
            }
            else if(new_node->priority > cur->next->priority){ // inserting in middle of linked list.
                new_node -> next = cur -> next;
                cur -> next = new_node;
                return;
            }
    
    
            cur = cur -> next;
        }
    }

    else if (priorityFlag == 0){    // time_in_s ordering
        printf("Ordering with delay_in_s\n");

        if (new_node -> delay_in_s < list -> head -> delay_in_s){  // inserting at the head
            new_node -> next = list -> head;
            list -> head = new_node;
            return;
        }
    
        Node *cur = list -> head;  // start at head of list
        
        while (cur != NULL){
    
            if (cur -> next == NULL){ // inserting in tail of linked list
                cur -> next = new_node;
                new_node -> next = NULL;
                return;
            }
            else if(new_node->delay_in_s < cur->next->delay_in_s){ // inserting in middle of linked list.
                new_node -> next = cur -> next;
                cur -> next = new_node;
                return;
            }    
            cur = cur -> next;
        }
    }

    printf("%d is not a valid priority Flag\n", priorityFlag);

}

Node *popQueue(linkedList *list){

    if(list -> head == NULL){
        return NULL;
    }

    else{
        Node *popNode = list -> head;
        list -> head = list -> head -> next;
        popNode -> next = NULL;

        return popNode;
    }
}

linkedList *createLinkedList(){
    linkedList* ptr = (linkedList *)malloc(sizeof(linkedList));
    if (ptr == NULL){
        printf("Malloc Failed\n");
        exit(1);
    }
    ptr->head = NULL;
    return ptr;
}

