#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct m61_metadata {
    unsigned long long size;            // number of bytes in allocation
    unsigned long long active_code;     // if equal to 1234 if allocation is not 'active'
    char* ptr_addr;                     // address of the pointer to the allocation
    const char* file;                   // file in which allocation was called
    int line;                           // line in which allocation was called
    struct m61_metadata* prev;          // pointer to previous node in doubly linked list
    struct m61_metadata* next;          // pointer to next node in doubly linked list
    int padding;                        // padding to keep struct with 8-bit alignment
};

typedef struct m61_footer {
    unsigned long long buffer_one;      // 8-byte buffer for overflow
    unsigned long long buffer_two;      // 8-byte buffer for overflow
} m61_footer;

// Global struct to keep track of statistics
struct m61_statistics global_stats;

// Head doubly linked list of struct m61_metadata
struct m61_metadata* metadata_head = NULL;

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.

    // Prevent integer overflow: check to make sure sz not greater than 2^32-1
    if (sz > SIZE_MAX - sizeof(struct m61_statistics) - sizeof(m61_footer)) {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return NULL;
    }

    // Initialize a footer to monitor boundary overwrite errors
    m61_footer footer = {1111, 2222};

    // Initialize struct to hold metadata (i.e. allocation size)
    struct m61_metadata metadata = {sz, 0, NULL, file, line, NULL, NULL, 0};

    struct m61_metadata* ptr = NULL;

    // Allocate pointer w/ extra space to accommodate metadata
    ptr = malloc(sizeof(struct m61_metadata) + sz + sizeof(m61_footer));

    // Track failed allocations
    if (!ptr) {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return ptr;
    }

    // Track other statistics
    global_stats.ntotal++;
    global_stats.nactive++;
    global_stats.total_size += sz;
    global_stats.active_size += sz;

    char* heap_min = (char*) ptr;
    char* heap_max = (char*) ptr + sz + sizeof(struct m61_metadata) + sizeof(m61_footer);
    if (!global_stats.heap_min || global_stats.heap_min >= heap_min) {
        global_stats.heap_min = heap_min;
    }

    if (!global_stats.heap_max || global_stats.heap_max <= heap_max) {
        global_stats.heap_max = heap_max;
    }

    // Update metadata with relevant information
    metadata.ptr_addr = (char*) (ptr + 1);

    // Store metadata at beginning of allocated pointer
    *ptr = metadata;

    if (metadata_head) {
        ptr->next = metadata_head;
        metadata_head->prev = ptr;
    }
    metadata_head = ptr;


    // Store footer at the end of allocated pointer
    m61_footer* footer_ptr = (m61_footer*) ((char*) (ptr + 1) + sz);
    *footer_ptr = footer;

    // Return pointer to requested memory
    return ptr + 1;
}

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    if (ptr) {
        struct m61_metadata* new_ptr = (struct m61_metadata*) ptr - 1;
        if ((char*) new_ptr >= global_stats.heap_min && (char*) new_ptr <= global_stats.heap_max) {
            if (new_ptr->active_code != 1234) {
                if (new_ptr->ptr_addr != (char*) ptr) {
                    printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n", file, line, ptr);

                    for (struct m61_metadata* metadata = metadata_head; metadata != NULL; metadata = metadata->next) {
                        if ((char*) ptr >= (char*) metadata && (char*) ptr <= (char*) (metadata + 1) + metadata->size + sizeof(m61_footer))
                            printf("  %s:%d: %p is %zu bytes inside a %llu byte region allocated here\n", 
                                    metadata->file, metadata->line, ptr, (char*) ptr - metadata->ptr_addr, metadata->size);
                    }
                    abort();
                }

                if (new_ptr->next) {
                    if (new_ptr->next->prev != new_ptr) {
                        printf("MEMORY BUG: %s%d: invalid free of pointer %p\n", file, line, ptr);
                        abort();
                    }
                }
                else if (new_ptr->prev) {
                    if (new_ptr->prev->next != new_ptr) {
                        printf("MEMEORY BUG: %s%d: invalid free of pointer %p\n", file, line, ptr);
                        abort();
                    }
                }
                else if ((new_ptr->prev && new_ptr->next) &&
                        (new_ptr->prev->next != new_ptr || new_ptr->next->prev != new_ptr)) {
                    printf("MEMORY BUG: %s%d: invalid free of pointer %p\n", file, line, ptr);
                    abort();
                }

                m61_footer* footer_ptr = (m61_footer*) ((char*) ptr + new_ptr->size);
                if (footer_ptr->buffer_one != 1111 || footer_ptr->buffer_two != 2222) {
                    printf("MEMORY BUG: %s:%d: detected wild write during free of pointer %p\n", file, line, ptr);
                    abort();
                }

                // Remove node from doubly linked list
                if (new_ptr->prev)
                   new_ptr->prev->next = new_ptr->next;
                else
                    metadata_head = new_ptr->next;
                if (new_ptr->next)
                    new_ptr->next->prev = new_ptr->prev;

                new_ptr->next = NULL;
                new_ptr->prev = NULL;

                // Keep track of statistics
                global_stats.nactive--;
                global_stats.active_size -= new_ptr->size;

                // Set code to indicate inactive allocation
                new_ptr->active_code = 1234;
                free(new_ptr);
            }
            else {
                printf("MEMORY BUG: %s:%d: invalid free of pointer %p\n", file, line, ptr);
                abort();
            }
        }
        else {
            printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n", file, line, ptr);
            abort();
        }
    }
}

void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    void* new_ptr = NULL;
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    if (ptr && new_ptr) {
        // Copy the data from `ptr` into `new_ptr`.
        // To do that, we must figure out the size of allocation `ptr`.
        // Your code here (to fix test012).
        struct m61_metadata* metadata = (struct m61_metadata*) ptr - 1;
        size_t old_sz = metadata->size;
        if (old_sz <= sz)
            memcpy(new_ptr, ptr, old_sz);
        else
            memcpy(new_ptr, ptr, sz);
    }
    m61_free(ptr, file, line);
    return new_ptr;
}

void* m61_calloc(size_t nmemb, size_t sz, const char* file, int line) {
    // Your code here (to fix test014).
    if (nmemb * sz < sz || nmemb * sz  < nmemb) {
        global_stats.nfail++;
        return NULL;
    }
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}

void m61_getstatistics(struct m61_statistics* stats) {
    // Your code here.
    // Set all statistics to zero
    bzero(stats, sizeof(struct m61_statistics));

    // Set all statistics from global statistics variable
    *stats = global_stats;
}

void m61_printstatistics(void) {
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}

void m61_printleakreport(void) {
    for (struct m61_metadata* metadata = metadata_head; metadata != NULL; metadata = metadata->next) {
        printf("LEAK CHECK: %s:%d: allocated object %p with size %llu\n", metadata->file, metadata->line, metadata->ptr_addr, metadata->size);
    }
}
