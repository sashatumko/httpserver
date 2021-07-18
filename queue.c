#include "queue.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

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
  size_t size;
  size_t nthreads;
  sem_t sem; // sem for blocking threads
  pthread_mutex_t mtx;
} queue_t;

// constructor 
queue new_queue(size_t n) {
  queue q = malloc(sizeof(queue_t));
  q->front = q->back = NULL;
  q->size = 0;
  q->nthreads = n;
  sem_init(&q->sem, 0, n);
  pthread_mutex_init(&q->mtx, NULL);
  return q;
}

// returns number of queued up elements
size_t size(queue q) { 
  
  if (q == NULL) {
    warn("queue: bad pointer");
    return -1;
  }

  pthread_mutex_lock(&q->mtx); /* critical */
  size_t s = q->size;
  pthread_mutex_unlock(&q->mtx); /* critical */

  return s; 
}

// returns 1 if queue is empty
int is_empty(queue q) { 

  if (q == NULL) {
    warn("queue: bad pointer");
    return -1;
  }

  pthread_mutex_lock(&q->mtx); /* critical */
  int ret = (q->size == 0); 
  pthread_mutex_unlock(&q->mtx); /* critical */

  return ret;
}

// insert a thread id onto the back of the queue
void enqueue(queue q, int id) {

  if (q == NULL) {
    warn("queue: bad pointer");
    return;
  }
  pthread_mutex_lock(&q->mtx); /* critical */

  if (q->size == 0) {
    q->front = q->back = new_node(id);
  } else {
    q->back->next = new_node(id);
    q->back = q->back->next;
  }

  q->size++;
  pthread_mutex_unlock(&q->mtx); /* critical */

  sem_post(&q->sem); // FLAG -> available
}

// pop and return thread id off the front of the queue 
// returns -1 if error
int dequeue(queue q) {

  sem_wait(&q->sem); // SLEEP IF NO THREADS AVAILABLE

  if (q == NULL) {
    warn("queue: dequeue err");
    return -1;
  }

  pthread_mutex_lock(&q->mtx); /* critical */
  int id = q->front->thread_id;
  node temp = q->front;
  if (q->size == 1) {
    q->front = q->back = NULL;
  } else {
    q->front = q->front->next;
  }

  free_node(&temp);
  q->size--;
  pthread_mutex_unlock(&q->mtx); /* critical */

  return id;
}

// clear the queue
void make_empty(queue q) {

  if (q == NULL) {
    warn("queue: bad pointer");
    return;
  }

  pthread_mutex_lock(&q->mtx); /* critical */
  while (q->size > 0) {
    dequeue(q);
  }
  pthread_mutex_unlock(&q->mtx); /* critical */

}

// destructor
void free_queue(queue *q) {

  if (q != NULL && *q != NULL) {
    pthread_mutex_lock(&(*q)->mtx); /* critical */
    make_empty(*q);
    pthread_mutex_unlock(&(*q)->mtx); /* critical */
    free(*q);
    *q = NULL;
  }

}

// print elements on queue
void print_queue(queue q) {

  pthread_mutex_lock(&q->mtx); /* critical */
  node head = q->front;
  while (head != NULL) {
    printf("%d ", head->thread_id);
    head = head->next;
  }
  printf("\n");
  pthread_mutex_unlock(&q->mtx); /* critical */

}