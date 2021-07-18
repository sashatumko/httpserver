#ifndef _QUEUE_H_INCLUDE_
#define _QUEUE_H_INCLUDE_
// #include <stdint.h>

/*
    QUEUE ADT
*/

// exported type
typedef struct queue_t *queue;

// constructor
queue new_queue();

// destructor
void free_queue(queue *Q);

// size of queue
int size(queue Q);

// check if empty queue 
int is_empty(queue Q);

// enqueue id onto back of queue Q
void enqueue(queue Q, int id);

// pop and return id from front of Q
int dequeue(queue Q);

// delete all elements from queue
void make_empty(queue Q);

// print all elements in queue
void print_queue(queue Q);

#endif