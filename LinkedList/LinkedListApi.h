#ifndef __LINKEDLIST__
#define __LINKEDLIST__


#define GET_HEAD_SINGLY_LL(ll) (ll->head)
#define INC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count++)
#define DEC_NODE_COUNT_SINGLY_LL(ll) (ll->node_count--)
#define GET_NODE_COUNT_SINGLY_LL(ll) (ll->node_count)
#define GET_NEXT_NODE_SINGLY_LL(node) (node->next)

typedef enum{
    LL_FALSE,
    LL_TRUE
} bool_t;

typedef struct LL_Node{
    void *data;
    struct LL_Node *next;
} singly_ll_node_t;

typedef struct LL{
    unsigned int node_count;
    singly_ll_node_t *head;
} ll_t;

ll_t* init_singly_ll();
singly_ll_node_t* singly_ll_init_node(void* data);
int singly_ll_add_node(ll_t *ll, singly_ll_node_t *node);
int singly_ll_add_node_by_val(ll_t *ll, void* data);
int singly_ll_remove_node(ll_t *ll, singly_ll_node_t *node);
unsigned int singly_ll_remove_node_by_value(ll_t *ll, void* data, int size);
bool_t is_singly_ll_empty(ll_t *ll);
void print_singly_LL(ll_t *ll);
void reverse_singly_ll(ll_t *ll);
void delete_singly_ll(ll_t *ll);
int singly_ll_delete_node(ll_t *ll, singly_ll_node_t *node);
unsigned int singly_ll_delete_node_by_value(ll_t *ll, void *data, int size);
singly_ll_node_t *singly_ll_get_node_by_data_ptr(ll_t *ll, void *data);
unsigned int singly_ll_remove_node_by_dataptr(ll_t *ll, void *data);
#endif
