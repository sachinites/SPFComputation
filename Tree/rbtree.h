enum rbtree_node_color { RED, BLACK };

typedef int (*compare_func)(void* left, void* right);
typedef void *(*key_match) (void *key, void *user_data);

typedef struct rbtree_node_t {
    void* key;
    struct rbtree_node_t* left;
    struct rbtree_node_t* right;
    struct rbtree_node_t* parent;
    enum rbtree_node_color color;
} *rbtree_node;

typedef struct rbtree_t {
    rbtree_node root;
    compare_func _compare_func;
    key_match _key_match;
    int gloffset;
} *rbtree;


/*public APIs*/
rbtree rbtree_create();
void* rbtree_lookup(rbtree t, void* key, compare_func compare);
void rbtree_insert(rbtree t, void* key, compare_func compare);
void rbtree_delete(rbtree t, void* key, compare_func compare);
typedef rbtree_node node;
node rbtree_get_next_node(node n);
node rbtree_get_first_node(rbtree t);
void rbtree_node_delete(rbtree t, node n);
void rbtree_flush(rbtree t);

/*GL functions*/
rbtree rbtree_glcreate(compare_func _compare_func, int offset, key_match _key_match);
void rbtree_glinsert(rbtree t, node n);
void rbtree_gldelete(rbtree t, node n);
void rbtree_glflush(rbtree t);

#define RBTREE_GET_USER_DATA(rbtreeptr, nodeptr)    \
    ((void *)(char *)(nodeptr) - (rbtreeptr)->gloffset)


#define ITERATE_RB_TREE_BEGIN(treeptr, nodeptr)   \
{                                                 \
    node _next_node = 0;                          \
    for(nodeptr = rbtree_get_first_node(treeptr); nodeptr; nodeptr = _next_node){   \
    _next_node = rbtree_get_next_node(nodeptr);

#define ITERATE_RB_TREE_END }}
/*public APIs*/



typedef enum rbtree_node_color color;


static node grandparent(node n);
static node sibling(node n);
static node uncle(node n);
static void verify_properties(rbtree t);
static void property_1(node root);
static void property_2(node root);
static color node_color(node n);
static void property_4(node root);
static void property_5(node root);
static void property_5_helper(node n, int black_count, int* black_count_path);

static node new_node(void* key, color node_color, node left, node right);
static node lookup_node(rbtree t, void* key, compare_func compare);
static void rotate_left(rbtree t, node n);
static void rotate_right(rbtree t, node n);

//the cases for insertion and deletion (the conditions tobe checked)

static void replace_node(rbtree t, node oldn, node newn);
static void insert_case1(rbtree t, node n);
static void insert_case2(rbtree t, node n);
static void insert_case3(rbtree t, node n);
static void insert_case4(rbtree t, node n);
static void insert_case5(rbtree t, node n);
static node maximum_node(node root);
static void delete_case1(rbtree t, node n);
static void delete_case2(rbtree t, node n);
static void delete_case3(rbtree t, node n);
static void delete_case4(rbtree t, node n);
static void delete_case5(rbtree t, node n);
static void delete_case6(rbtree t, node n);

static node slip_left(node n);

