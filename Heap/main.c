#include "heap.h"
#include<stdio.h>

int
main(int argc, char **argv){

    struct heap h;
    heap_init(&h);
    unsigned int a3 = 3, a1 =1, a5 = 5, a8 = 8;
    heap_push(&h, &a3);
    heap_push(&h, &a1);
    heap_push(&h, &a5);
    heap_push(&h, &a8);

    printf("%u\n", heap_front(&h));
    heap_pop(&h);
    printf("heap count = %d\n", h.count);
    printf("%u\n", heap_front(&h));
    heap_pop(&h);
    printf("heap count = %d\n", h.count);
    printf("%u\n", heap_front(&h));
    heap_pop(&h);
    printf("heap count = %d\n", h.count);
    printf("%u\n", heap_front(&h));
    heap_pop(&h);
    printf("heap count = %d\n", h.count);
    if(!is_heap_empty(&h))
        heap_pop(&h);
    return 0;
}
