
typedef unsigned int * type;

#define HEAP_DEFAULT_SIZE   8

struct heap
{
	unsigned int size; // Size of the allocated memory (in number of items)
	unsigned int count; // Count of the elements in the heap
	type *data; // Array with the elements
};

void heap_init(struct heap * h);
void heap_push(struct heap * h, type value);
void heap_pop(struct heap * h);
void re_init_heap(struct heap * h);
void free_heap(struct heap * h);
// Returns the biggest element in the heap
#define heap_front(h) (*(h)->data)

// Frees the allocated memory
#define heap_term(h) (free((h)->data))

void heapify(type data[], unsigned int count);
int is_heap_empty(struct heap * h);
