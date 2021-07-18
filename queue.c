#include "queue.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

/* private node type */

typedef struct node_t {
  int thread_id;
  struct node_t *next;
} node_t;

typedef node_t *node;

// constructor 
node new_node(int id) {
  node n = (node)malloc(sizeof(node_t));
  n->thread_id = id;
  n->next = NULL;
  return n;
}

// destructor
void free_node(node *n) {
  if (n != NULL && *n != NULL) {
    free(*n);
    *n = NULL;
  }
}

/* queue type */

typedef struct queue_t {
  node front;
  node back;
  int size;
} queue_t;

// constructor 
queue new_queue() {
  queue q = malloc(sizeof(queue_t));
  q->front = q->back = NULL;
  q->size = 0;
  return q;
}

// returns number of queued up elements
int size(queue q) { 

  if (q == NULL) {
    warn("queue: bad pointer");
    return -1;
  }

  return (q->size); 
}

// returns 1 if queue is empty
int is_empty(queue q) { 

  if (q == NULL) {
    warn("queue: bad pointer");
    return -1;
  }

  return (q->size == 0); 
}

// insert a thread id onto the back of the queue
void enqueue(queue q, int id) {

  if (q == NULL) {
    warn("queue: bad pointer");
    return;
  }

  if (q->size == 0) {
    q->front = q->back = new_node(id);
  } else {
    q->back->next = new_node(id);
    q->back = q->back->next;
  }

  q->size++;
}

// pop and return thread id off the front of the queue 
// returns -1 if error
int dequeue(queue q) {

  if (q == NULL || q->size == 0) {
    warn("queue: dequeue err");
    return -1;
  }

  int id = q->front->thread_id;
  node temp = q->front;
  if (q->size == 1) {
    q->front = q->back = NULL;
  } else {
    q->front = q->front->next;
  }

  free_node(&temp);
  q->size--;

  return id;
}

// clear the queue
void make_empty(queue q) {

  if (q == NULL) {
    warn("queue: bad pointer");
    return;
  }

  while (q->size > 0) {
    dequeue(q);
  }

}

// destructor
void free_queue(queue *q) {

  if (q != NULL && *q != NULL) {
    make_empty(*q);
    free(*q);
    *q = NULL;
  }

}

// print elements on queue
void print_queue(queue q) {

  node head = q->front;
  while (head != NULL) {
    printf("%d ", head->thread_id);
    head = head->next;
  }
  printf("\n");

}