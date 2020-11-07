#ifndef _QUEUE_H_INCLUDE_
#define _QUEUE_H_INCLUDE_

// Exported type --------------------------------------------------------------
typedef struct QueueObj* Queue;

// Returns reference to new empty Queue object. 
Queue newQueue(void);

// Frees all heap memory associated with Queue *pQ, and sets *pQ to NULL.
void freeQueue(Queue* pQ);

// Returns the value at the front of Q.
// Pre: !isEmpty(Q)
int getFront(Queue Q);

// Returns true (1) if Q is empty, otherwise returns false (0)
int isEmpty(Queue Q);

// Places new data element at the end of Q
void Enqueue(Queue Q, int data);

// Deletes element at front of Q
// Pre: !isEmpty(Q)
void Dequeue(Queue Q);

#endif