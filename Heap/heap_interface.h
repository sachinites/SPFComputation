#include "heap.h"

typedef struct heap candidate_tree_t;

#define GET_NODE_PTR_FROM_METRIC_PTR(uint_ptr, level)      \
    (node_t *)((char *)uint_ptr - (unsigned int)&(((node_t *)0)->spf_metric[level]))

#define REMOVE_CANDIDATE_TREE_TOP(heap_ptr)         \
    heap_pop(heap_ptr)

#define GET_CANDIDATE_TREE_TOP(heap_ptr, level)            \
    GET_NODE_PTR_FROM_METRIC_PTR(heap_front(heap_ptr), level)

#define INSERT_NODE_INTO_CANDIDATE_TREE(heap_ptr, node_ptr, level)   \
    heap_push(heap_ptr, &(node_ptr->spf_metric[level]))

#define IS_CANDIDATE_TREE_EMPTY(heap_ptr)   \
    is_heap_empty(heap_ptr)

#define RE_INIT_CANDIDATE_TREE(heap_ptr)    \
    re_init_heap(heap_ptr)

#define CANDIDATE_TREE_INIT(heap_ptr)       \
    heap_init(heap_ptr)

#define FREE_CANDIDATE_TREE_INTERNALS(heap_ptr)     \
    assert(!(heap_ptr)->count);                     \
    free_heap(heap_ptr);
