#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

struct m61_metadata {
    unsigned long long size;    // number of bytes in allocation
};

struct m61_statistics global_stats; // ***Should this be initialized?***

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    
    if (sz >= (size_t) -1)
    {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return NULL;
    }
    
    struct m61_metadata metadata = {sz};    // ***Is this a clear why to initialize this struct?***
    struct m61_metadata* ptr = NULL;       // ***Should this be initialized to NULL? (See m61_realloc below)***
    ptr = malloc(sizeof(struct m61_metadata) + sz);
    if (!ptr) {
        global_stats.nfail++;
        global_stats.fail_size += sz;
        return ptr;
    }
    global_stats.ntotal++;
    global_stats.nactive++;
    global_stats.total_size += sz;
    global_stats.active_size += sz;
    char* heap_min = (char*) ptr;
    char* heap_max = (char*) (ptr + sz);
    if (!global_stats.heap_min || global_stats.heap_min >= heap_min) {
        global_stats.heap_min = heap_min;
    }

    if (!global_stats.heap_max || global_stats.heap_max <= heap_max) {
        global_stats.heap_max = heap_max;
    }
    *ptr = metadata;
    return ptr + sizeof(struct m61_metadata);
}

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    global_stats.nactive--;
    struct m61_metadata* new_ptr = (struct m61_metadata*) ptr - sizeof(struct m61_metadata);    // ***Why did I need to cast this?***
    global_stats.active_size -= new_ptr->size; 
    free(new_ptr);
}

void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    void* new_ptr = NULL;
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    if (ptr && new_ptr) {
        // Copy the data from `ptr` into `new_ptr`.
        // To do that, we must figure out the size of allocation `ptr`.
        // Your code here (to fix test012).
    }
    m61_free(ptr, file, line);
    return new_ptr;
}

void* m61_calloc(size_t nmemb, size_t sz, const char* file, int line) {
    // Your code here (to fix test014).
    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr)
        memset(ptr, 0, nmemb * sz);
    return ptr;
}

void m61_getstatistics(struct m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(struct m61_statistics));
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
