#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../queue.h"

int main() {

    queue q = new_queue();

    assert(size(q) == 0 && is_empty(q));

    for(int i = 0; i < 10; ++i) {
        enqueue(q, i);
    }

    assert(size(q) == 10 && !is_empty(q));
    print_queue(q);

    make_empty(q);
    assert(size(q) == 0 && is_empty(q));

    for(int i = 0; i < 5; ++i) {
        enqueue(q, i+100);
    }

    while(!is_empty(q)) {
        printf("dequeued: %d\n", dequeue(q));
    }

    // try to dequeue empty queue
    assert(dequeue(q) == -1);
    printf("dequeued: %d\n", dequeue(q));

    free_queue(&q);

    return 0;
}