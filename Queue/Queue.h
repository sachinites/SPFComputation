#ifndef __QUEUE__
#define __QUEUE__


#define Q_DEFAULT_SIZE  50
typedef struct _Queue{
        void *elem[Q_DEFAULT_SIZE];
        unsigned int front;
        unsigned int rear;
        unsigned int count;
} Queue_t;

Queue_t* initQ();

int
is_queue_empty(Queue_t *q);

int
is_queue_full(Queue_t *q);

int
enqueue(Queue_t *q, void *ptr);

void*
deque(Queue_t *q);

void
print_Queue(Queue_t *q);

void
reuse_q(Queue_t *q);
#endif
