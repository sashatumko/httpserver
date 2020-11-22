#ifndef _QUEUE_H_INCLUDE_
#define _QUEUE_H_INCLUDE_
#include <cstdint>

/*
    QUEUE ADT
*/

typedef struct queue_t *queue;

queue new_queue();

int size(queue Q);

bool is_empty(queue Q);

void enqueue(queue Q, int id);

int dequeue(queue Q);

void make_empty(queue Q);

void free_queue(queue *Q);

void print_queue(queue Q);

#endif