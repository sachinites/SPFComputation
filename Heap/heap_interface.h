#include "heap.h"

typedef struct heap candidate_tree_t;

#define GET_NODE_PTR_FROM_METRIC_PTR(uint_ptr)      \
    (node_t *)&((char *)uint_ptr - (unsigned int)&(node_t *)0->spf_metric)

#define REMOVE_CANDIDATE_TREE_TOP(heap_ptr)         \
    heap_pop(heap_ptr)

#define GET_CANDIDATE_TREE_TOP(heap_ptr)            \
    GET_NODE_PTR_FROM_METRIC_PTR(heap_front(heap_ptr))

#define INSERT_NODE_INTO_CANDIDATE_TREE(heap_ptr, node_ptr)   \
    heap_push(heap_ptr, &(node_ptr->spf_metric))

#define IS_CANDIDATE_TREE_EMPTY(heap_ptr)   \
    is_heap_empty(heap_ptr)

#define RE_INIT_CANDIDATE_TREE(heap_ptr)    \
    re_init_heap(heap_ptr)

#define CANDIDATE_TREE_INIT(heap_ptr)       \
    heap_init(heap_ptr)

#define FREE_CANDIDATE_TREE_INTERNALS(heap_ptr)   \
    free_heap(heap_ptr);
