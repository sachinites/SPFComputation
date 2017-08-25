#include "heap.h"

#define GET_NODE_PTR_FROM_METRIC_PTR(int_ptr)  \
    (node_t *)&((char *)int_ptr - (unsigned int)&(node_t *)0->spf_metric)

#define REMOVE_HEAP_TOP(heap_ptr)   \
    heap_pop(heap_ptr);

#define GET_HEAP_TOP(heap_ptr)  \
    GET_NODE_PTR_FROM_METRIC_PTR(heap_front(heap_ptr));

#define INSERT_NODE_INTO_HEAP(heap_ptr, node_ptr)   \
    heap_push(heap_ptr, &(node_ptr->spf_metric));
