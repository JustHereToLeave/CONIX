#include "heap.h"
#include "terminal.h"

#include "heap.h"
#include "terminal.h"

// One header sits immediately before every block, free or in-use.
typedef struct block_header {
    size_t size;                // size of usable memory in this block (not counting header)
    int free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

#define HEAP_SIZE (1024 * 1024) // 1MB, same starting pool as before
static uint8_t heap_pool[HEAP_SIZE];
static block_header_t *heap_start = NULL;

#define ALIGN 16
#define ALIGN_UP(x) (((x) + (ALIGN - 1)) & ~(ALIGN - 1))
#define HEADER_SIZE (sizeof(block_header_t))

void heap_init(void) {
    heap_start = (block_header_t *)heap_pool;
    heap_start->size = HEAP_SIZE - HEADER_SIZE;
    heap_start->free = 1;
    heap_start->next = NULL;
    heap_start->prev = NULL;
}

// Split `block` if it's big enough to carve out `size` bytes plus
// leave a viable leftover block. Otherwise the caller just gets the
// whole block (a little internal waste beats an unusable sliver).
static void split_block(block_header_t *block, size_t size) {
    size_t remaining = block->size - size;

    if (remaining <= HEADER_SIZE + ALIGN) {
        return; // not worth splitting, leftover would be too small to ever use
    }

    block_header_t *new_block = (block_header_t *)((uint8_t *)block + HEADER_SIZE + size);
    new_block->size = remaining - HEADER_SIZE;
    new_block->free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (block->next) {
        block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = size;
}

void *kmalloc(size_t size) {
    if (!heap_start) heap_init();
    if (size == 0) return NULL;

    size = ALIGN_UP(size);

    // First-fit search: walk the list, take the first free block big enough.
    block_header_t *cur = heap_start;
    while (cur) {
        if (cur->free && cur->size >= size) {
            split_block(cur, size);
            cur->free = 0;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    return NULL; // out of memory
}

// Merge `block` with its next neighbor if that neighbor is also free.
static void coalesce_forward(block_header_t *block) {
    block_header_t *next = block->next;
    if (next && next->free) {
        block->size += HEADER_SIZE + next->size;
        block->next = next->next;
        if (next->next) {
            next->next->prev = block;
        }
    }
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    block->free = 1;

    // Try merging with both neighbors so freed space stays usable
    // as one contiguous chunk instead of fragmenting.
    coalesce_forward(block);
    if (block->prev && block->prev->free) {
        coalesce_forward(block->prev);
    }
}
