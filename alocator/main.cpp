#include <stddef.h>
#include <stdint.h>

static const size_t HEAP_SIZE  = 4096;
static const size_t SMALL_SIZE = 15;
static const size_t LARGE_SIZE = 180;

/* Заголовок блока памяти */
struct block {
    struct block* next;
    uint8_t size; 
};

static uint8_t heap[4096];
static struct block* free_list = NULL;
static int initialized = 0;

/* Инициализация пула памяти */
static void init_heap(void) {
    uint8_t* ptr = heap;

    while (ptr + sizeof(struct block) + SMALL_SIZE <= heap + HEAP_SIZE) {
        struct block* b = (struct block*)ptr;

        if (ptr + sizeof(struct block) + LARGE_SIZE <= heap + HEAP_SIZE) {
            b->size = (uint8_t)LARGE_SIZE;
            ptr += sizeof(struct block) + LARGE_SIZE;
        } else {
            b->size = (uint8_t)SMALL_SIZE;
            ptr += sizeof(struct block) + SMALL_SIZE;
        }

        b->next = free_list;
        free_list = b;
    }
}

/* Выделение блока памяти заданного размера */
void* my_malloc(size_t size) {
    if (!initialized) {
        init_heap();
        initialized = 1;
    }

    struct block** prev = &free_list;
    struct block* cur = free_list;

    while (cur) {
        if (cur->size == size) {
            *prev = cur->next; 
            return (uint8_t*)cur + sizeof(struct block);
        }
        prev = &cur->next;
        cur = cur->next;
    }

    return NULL;
}

/* Освобождение блока памяти */
void my_free(void* ptr) {
    if (!ptr)
        return;

    struct block* b =
        (struct block*)((uint8_t*)ptr - sizeof(struct block));

    b->next = free_list;
    free_list = b;
}

int main(void) {
    void* a = my_malloc(15);
    void* b = my_malloc(180);

    if (!a || !b)
        return 1;

    my_free(a);
    my_free(b);

    return 0;
}
