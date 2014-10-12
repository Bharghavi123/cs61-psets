#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define CACHE_SIZE 16384
// INVARIANT: assert(cache->start <= cache->size);
// CACHE INVALID / EMPTY: cache->size == 0 => cache->start == cache->size

// io61.c
//    YOUR CODE HERE!

typedef struct io61_cache {
    unsigned char* memory;
    size_t size;     // Current size of cache
    size_t first;    // Position of the first char in writing cache
    size_t last;     // Position of the last char in writing cache
    off_t start;     // File offset of first character in cache
    off_t next;      // File offset of next char to read in cache
    off_t prev_next; // File offset of previous next
    off_t end;       // File offset of last valid char in cache
} io61_cache;


// io61_file
//    store structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    io61_cache* cache;
    int mode;
};

// min(a, b)
// Returns the minimum of a and b
int min(int a , int b) {
    return (a < b) ? a : b;
}

// make_cache()
//    Returns pointer to instantiated cache 
io61_cache* make_cache() {
    io61_cache* cache = malloc(sizeof(io61_cache));
    cache->memory = calloc(CACHE_SIZE, sizeof(char));
    cache->size = cache->first = cache->last = cache->start = cache->next = cache->prev_next = cache->end = 0;
    return cache;
}

// io61_fdopen(fd, mode)
//    Return a new io61_file that reads from and/or writes to the given
//    file descriptor `fd`. `mode` is either O_RDONLY for a read-only file
//    or O_WRONLY for a write-only file. You need not support read/write
//    files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = (io61_file*) malloc(sizeof(io61_file));
    f->fd = fd;
    f->cache = make_cache();
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Close the io61_file `f` and release all its resources, including
//    any buffers.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    free(f);
    return r;
}


// io61_readc(f)
//    Read a single (unsigned) character from `f` and return it. Returns EOF
//    (which is -1) on error or end-of-file.

int io61_readc(io61_file* f) {
    // Alias for file cache
    io61_cache* cache = f->cache;
    // If the cache is valid...
    if (cache->next < cache->end) {
        // Update the cache position
        cache->next++;
        // Return the next char from cache
        return *(cache->memory + cache->next - cache->start - 1);
    }
    // The cache is invalid/empty
    else {
        // Mark the cache as valid
        cache->start = cache->end;

        // Read directly from the file
        ssize_t size = read(f->fd, cache->memory, CACHE_SIZE);
        
        // If able to read more than 0...
        if (size > 0) {
            // ...update the cache end offset
            cache->end += size;
            // Update the cache position
            cache->next++;
            // Read the next char from cache
            return *(cache->memory + cache->next - cache->start - 1);
        }
        else
            return EOF;
    }
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count if the file ended before `sz` characters could be read. Returns
//    -1 an error occurred before any characters were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    // Alias for f->cache
    io61_cache* cache = f->cache;

    size_t nread = 0; // number of characters read so far
    
    while (nread != sz) {
        // If the cache is valid...
        if (cache->next < cache->end) {
            // Determine the size to read from the cache
            ssize_t size = min(cache->end - cache->next, sz - nread);
            
            // Copy from cache into the buffer
            memcpy(buf + nread, cache->memory +  cache->next - cache->start, size);

            // Update the cache position
            cache->next += size;

            // Update the amount read from the cache
            nread += size;
        }
        // Else, the cache is invalid/empty...
        else {
            // Mark the cache as valid
            cache->start = cache->end;
            // Read directly from file
            ssize_t size = read(f->fd, cache->memory, CACHE_SIZE);
            // If able to read more than 0...
            if (size > 0)
                cache->end += size; // ...update the cache end offset
            else
                return (ssize_t) nread ? (ssize_t) nread : size;
        }
    }
    return nread;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    // Alias the file cache
    io61_cache* cache = f->cache;

    const char* buf = (const char*) &ch;

    // Determine how much space is left in cache
    size_t available = CACHE_SIZE - cache->size;

    // If cache is already full...
    if (!available) {
        //...write the cache to the file.
        ssize_t cleared = write(f->fd, cache->memory + cache->first, CACHE_SIZE - cache->first);
        // If able to write some of cache to file...
        if (cleared >= 0) {
            cache->first += cleared;
            cache->size -= cleared;
            // Check to see if last or first at the end of cache
            cache->first = (CACHE_SIZE == cache->first) ? 0 : cache->first;
            cache->last = (CACHE_SIZE == cache->last) ? 0 : cache->last;
        }
        // Else write not successful
        else
            return -1;   
    }
    // Write char to cache...
    *(cache->memory + cache->last) = *buf;
    // Update position of last char in cache
    cache->last++;
    // Update cache size
    cache->size++;
    return 0;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    // Create an alias for the file cache
    io61_cache* cache = f->cache;
   
    size_t nwritten = 0;

    while (nwritten != sz) {

        // Determine how much space is left in cache 
        size_t available = CACHE_SIZE - cache->size;

        // If the previous next is not the same as the current next...
        if (cache->prev_next != cache->next) {
            //...seek back to previous next
            off_t r = lseek(f->fd, cache->prev_next, SEEK_SET);
            if (r != cache->prev_next)
                return -1;
            //...write the cache to file
            ssize_t cleared = write(f->fd, cache->memory + cache->first, cache->size);
            
            if (cleared >= 0) {
                // update the previous next to next, if all of the cache was read 
                cache->prev_next = ((size_t) cleared != cache->size) ? (cache->prev_next + cleared) : cache->next;
                // ...update cache first, end, and size
                cache->first += cleared;
                cache->size -= cleared;
                // Check to see if last or first at the end of cache
                cache->first = (CACHE_SIZE == cache->first) ? 0 : cache->first;
                cache->last = (CACHE_SIZE == cache->last) ? 0 : cache->last;
                // ...see back to next
                lseek(f->fd, cache->next, SEEK_SET);
                off_t r = lseek(f->fd, cache->prev_next, SEEK_SET);
                if (r != cache->prev_next)
                    return (ssize_t) nwritten ? (ssize_t) nwritten : cleared;
            }
            // Else write not successful
            else
                return (ssize_t) nwritten ? (ssize_t) nwritten : cleared; 
        }
        // If cache already full...
        else if (!available) {
            //...write the cache to the file.
            ssize_t cleared = write(f->fd, cache->memory + cache->first, CACHE_SIZE - cache->first);
            // If able to write some of cache to file...
            if (cleared >= 0) {
                // ...update cache first, end, and size
                cache->first += cleared;
                cache->size -= cleared;
                // Check to see if last or first at the end of cache
                cache->first = (CACHE_SIZE == cache->first) ? 0 : cache->first;
                cache->last = (CACHE_SIZE == cache->last) ? 0 : cache->last;
            }
            // Else write not successful
            else
                return (ssize_t) nwritten ? (ssize_t) nwritten : cleared;             
        }
        // Else, there is space in the cache...
        else {
            //...determine how much to write into the cache
            ssize_t size = min(sz - nwritten, available); 
            //...write into the cache
            memcpy(cache->memory + cache->last, buf + nwritten, size);
            // ... update the cache last
            cache->last += size;
            // ...update the cache size
            cache->size += size;
            //...and update how much as been written
            nwritten += size;
        }
    }
    return nwritten;
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    // Alias for file cache
    io61_cache* cache = f->cache;
    // While the cache is not empty...
    while (cache->size) {
        size_t size = cache->first ? CACHE_SIZE - cache->first : cache->last;
        //...write the cache to the file.
        ssize_t cleared = write(f->fd, cache->memory + cache->first, size);
        // If able to write some of cache to file...
        if (cleared > 0) {
            // ...update cache position and size
            cache->first += cleared;
            cache->size -= cleared;
        }
        // Else if nothing was written...
        else if (cleared == 0) {
            cache->first = cleared;
        }
        // Else not able to write successfully       
        else
            return -1;
    }
    return 0;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    // Alias the cache
    io61_cache* cache = f->cache;
    if (pos < cache->start || pos > cache->end) {
        off_t aligned_off = pos - (pos % CACHE_SIZE);
        off_t r = lseek(f->fd, aligned_off, SEEK_SET);
        if (r != aligned_off)
            return -1;
        cache->start = cache->end = aligned_off;
    }
    cache->prev_next = cache->next;
    cache->next = pos;
    return 0;
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Open the file corresponding to `filename` and return its io61_file.
//    If `filename == NULL`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != NULL` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename)
        fd = open(filename, mode, 0666);
    else if ((mode & O_ACCMODE) == O_RDONLY)
        fd = STDIN_FILENO;
    else
        fd = STDOUT_FILENO;
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_filesize(f)
//    Return the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode))
        return s.st_size;
    else
        return -1;
}


// io61_eof(f)
//    Test if readable file `f` is at end-of-file. Should only be called
//    immediately after a `read` call that returned 0 or -1.

int io61_eof(io61_file* f) {
    char x;
    ssize_t nread = read(f->fd, &x, 1);
    if (nread == 1) {
        fprintf(stderr, "Error: io61_eof called improperly\n\
  (Only call immediately after a read() that returned 0 or -1.)\n");
        abort();
    }
    return nread == 0;
}
