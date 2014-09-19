#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct m61_metadata {
    unsigned long long size;    // number of bytes in allocation
    unsigned long long active_code;            // if equal to 1234 if allocation is not 'active'
};

struct m61_statistics global_stats; // ***Should this be initialized?***

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    
    // Prevent integer overflow: check to make sure sz not greater than 2^32-1
    if (sz > SIZE_MAX - sizeof(struct m61_statistics)) {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return NULL;
    }
    
    // Initialize struct to hold metadata (i.e. allocation size)
    struct m61_metadata metadata = {sz, 0};

    struct m61_metadata* ptr = NULL;

    // Allocate pointer w/ extra space to accommodate metadata
    ptr = malloc(sizeof(struct m61_metadata) + sz);

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
    char* heap_max = (char*) ptr + sz + sizeof(struct m61_metadata);
    if (!global_stats.heap_min || global_stats.heap_min >= heap_min) {
        global_stats.heap_min = heap_min;
    }

    if (!global_stats.heap_max || global_stats.heap_max <= heap_max) {
        global_stats.heap_max = heap_max;
    }

    // Store metadata at beginning of allocated pointer
    *ptr = metadata;

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
                global_stats.nactive--;
                global_stats.active_size -= new_ptr->size;
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
    // Your code here.
}
