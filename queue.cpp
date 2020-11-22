#include "queue.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KEY_SIZE 256

// element type (for queue)
typedef struct element_t {
  int thread_id;
  element_t *next;
} element_t;

// node type for queue elements
typedef element_t *element;

// new queue element
element new_element(int id) {
  element E = (element)malloc(sizeof(element_t));
  E->thread_id = id;
  E->next = NULL;
  return E;
}

// free element
void free_element(element *E) {
  if (E != NULL && *E != NULL) {
    free(*E);
    *E = NULL;
  }
}

typedef struct queue_t {
  element front;
  element back;
  int size;
} queue_t;

typedef queue_t *queue;

queue new_queue() {
  queue Q = (queue)malloc(sizeof(queue_t));
  Q->front = NULL;
  Q->back = NULL;
  Q->size = 0;
  return Q;
}

int size(queue Q) { return (Q->size); }

bool is_empty(queue Q) { return (Q->size == 0); }

void enqueue(queue Q, int id) {
  if (Q->size == 0) {
    Q->front = Q->back = new_element(id);
  } else {
    Q->back->next = new_element(id);
    Q->back = Q->back->next;
  }
  Q->size++;
}

int dequeue(queue Q) {
  if (Q->size == 0) {
    warn("cant dequeue - already empty");
    return -1;
  }

  int id = Q->front->thread_id;
  element temp = Q->front;
  if (Q->size == 1) {
    Q->front = Q->back = NULL;
  } else {
    Q->front = Q->front->next;
  }
  free_element(&temp);
  Q->size--;
  return id;
}

void make_empty(queue Q) {
  while (Q->size > 0) {
    dequeue(Q);
  }
}

void free_queue(queue *Q) {
  if (Q != NULL && *Q != NULL) {
    make_empty(*Q);
    free(*Q);
    *Q = NULL;
  }
}

void print_queue(queue Q) {
  element head = Q->front;
  while (head != NULL) {
    printf("%d ", head->thread_id);
    head = head->next;
  }
  printf("\n");
}