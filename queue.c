#include<stdio.h>
#include<stdlib.h>
#include "queue.h"

typedef struct NodeObj{
   int data;
   struct NodeObj* next;
} NodeObj;

typedef NodeObj* Node;

typedef struct QueueObj{
   Node front;
   Node back;
   int length;
} QueueObj;

Node newNode(int data){
   Node N = malloc(sizeof(NodeObj));
   N->data = data;
   N->next = NULL;
   return(N);
}

void freeNode(Node* pN){
   if( pN!=NULL && *pN!=NULL ){
      free(*pN);
      *pN = NULL;
   }
}

Queue newQueue(void){
   Queue Q;
   Q = malloc(sizeof(QueueObj));
   Q->front = Q->back = NULL;
   Q->length = 0;
   return(Q);
}

void freeQueue(Queue* pQ){
   if(pQ!=NULL && *pQ!=NULL) { 
      while( !isEmpty(*pQ) ) { 
         Dequeue(*pQ); 
      }
      free(*pQ);
      *pQ = NULL;
   }
}

int getFront(Queue Q){
   if( Q==NULL ){
      printf("Queue Error: calling getFront() on NULL Queue reference\n");
      exit(1);
   }
   if( isEmpty(Q) ){
      printf("Queue Error: calling getFront() on an empty Queue\n");
      exit(1);
   }
   return(Q->front->data);
}

int getLength(Queue Q){
   if( Q==NULL ){
      printf("Queue Error: calling getLength() on NULL Queue reference\n");
      exit(1);
   }
   return(Q->length);
}

int isEmpty(Queue Q){
   if( Q==NULL ){
      printf("Queue Error: calling isEmpty() on NULL Queue reference\n");
      exit(1);
   }
   return(Q->length==0);
}

void Enqueue(Queue Q, int data)
{
   Node N = newNode(data);

   if( Q==NULL ){
      printf("Queue Error: calling Enqueue() on NULL Queue reference\n");
      exit(1);
   }
   if( isEmpty(Q) ) { 
      Q->front = Q->back = N; 
   }else{ 
      Q->back->next = N; 
      Q->back = N; 
   }
   Q->length++;
}

void Dequeue(Queue Q){
   Node N = NULL;

   if( Q==NULL ){
      printf("Queue Error: calling Dequeue() on NULL Queue reference\n");
      exit(1);
   }
   if( isEmpty(Q) ){
      printf("Queue Error: calling Dequeue on an empty Queue\n");
      exit(1);
   }
   N = Q->front;
   if( getLength(Q)>1 ) { 
      Q->front = Q->front->next; 
   }else{ 
      Q->front = Q->back = NULL; 
   }
   Q->length--;
   freeNode(&N);
}