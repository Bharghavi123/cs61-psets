#include "io61.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>

#define CACHE_SIZE 32768

// io61.c
//    YOUR CODE HERE!

typedef struct io61_cache {
    size_t start;
    size_t end;
    unsigned char* memory;
    size_t size;
} io61_cache;


// io61_file
//    store structure for io61 file wrappers. Add your own stuff.

struct io61_file {
    int fd;
    off_t position;       // Actual position in file
    off_t user_position;  // User 'position' in file
    io61_cache* cache;
};

// make_cache()
//    Returns pointer to instantiated cache 
io61_cache* make_cache() {
    io61_cache* cache = malloc(sizeof(io61_cache));
    cache->memory = calloc(CACHE_SIZE, sizeof(char));
    cache->start = 0;
    cache->end = 0;
    cache->size = 0;
    return cache;
}

// update_cache(f)
//    Update the current cache, returns number of bytes put in cash

size_t update_cache(io61_file* f) {
    size_t nread = 0;
    while (nread != CACHE_SIZE) {
        ssize_t actual_read = read(f->fd, f->cache->memory + nread, CACHE_SIZE - nread);
        if (actual_read == 0)
            break;
        nread += (ssize_t) actual_read;
    }
    f->cache->start = (size_t) f->position;
    f->position += (off_t) nread;
    f->cache->end = (size_t) f->position;
    return nread;
}


// cache_read(cache, buffer, position, sz)
//    Attempts to read from the cache. Returns the amount read from the cache
//    and returns 0 otherwise.
   
size_t cache_read(io61_cache* cache, char* buffer, off_t position, size_t sz) {
    off_t cache_start = cache->start;
    off_t cache_end = cache->end;

    if (position >= cache_start && position + (off_t) sz <= cache_end) {
        memcpy(buffer, cache->memory + (position - cache_start), sz);
        return sz;
    }
    else if (position >= cache_start && position <= cache_end && position + (off_t) sz > cache_end) {
        // Read what we can from our cache
        size_t available = cache_end - position;
        memcpy(buffer, cache->memory + (position - cache_start), available);
        // Amount that we were able to read from cache
        return available;
    }
    // We weren't able to read anything from cache
    return 0;
}

// write_cache(cache, buffer, sz)
//    Attempts to write to cache. Returns the amount written to the cache,
//    otherwise returns 0 if cache is full and can't be written to

size_t write_cache(io61_cache* cache, const char* buffer, size_t sz) {
    // Determine how much space is left in our cache 
    size_t available = CACHE_SIZE - cache->size;

    // Check if the cache is already full
    if (!available) {
        return 0;
    }

    // Determine how much to actually write
    size_t nwritten = available >= sz ? sz : available;

    // If we still have some space, write whatever we can to the cache
    memcpy(cache->memory + cache->size, buffer, nwritten);

    // Update the size of our cache
    cache->size += nwritten;

    // Return the amount that was actually written
    return nwritten;
}

// empty_cache(f)
//    Empty the current cache by writing to f, returns void

void empty_cache(io61_file *f) {
    size_t nwritten = 0;
    while (nwritten != f->cache->size) {
        ssize_t actual_written = write(f->fd, f->cache->memory + nwritten, f->cache->size - nwritten);
        if (actual_written == -1)
            break;
        nwritten += (size_t) actual_written;
    }

    // Update cache size
    f->cache->size -= nwritten;
    
    // Copy the unwritten cache to the beginning of the cache
    memcpy(f->cache->memory, f->cache->memory + nwritten, f->cache->size);
    
    // Zero the cache that was written
    bzero(f->cache->memory + f->cache->size, nwritten);
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
    f->position = 0;
    f->user_position = 0;
    f->cache = make_cache();
    (void) mode;
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
    unsigned char buf[1];
    char* buffer = malloc(sizeof(char));
    if (io61_read(f, buffer, 1) == 1) {
        buf[0] = (unsigned char) *buffer;
        free(buffer);
        return buf[0];
    }
    else
        return EOF;
}


// io61_read(f, buf, sz)
//    Read up to `sz` characters from `f` into `buf`. Returns the number of
//    characters read on success; normally this is `sz`. Returns a short
//    count if the file ended before `sz` characters could be read. Returns
//    -1 an error occurred before any characters were read.

ssize_t io61_read(io61_file* f, char* buf, size_t sz) {
    size_t nread = 0;
    while (nread != sz) {

        // First, let's try to read from out cache        
        size_t actual_read = cache_read(f->cache, buf + nread, f->user_position, sz - nread);
            
        // Keep track of how much we've read
        nread += actual_read;

        // Update the user position
        f->user_position += (off_t) actual_read;

        // If we aren't able to read everything from our cache
        // we have to read from the file directly
        // Check for the end of file or if we already read enough, 
        // we can update cash was not updated
        if (nread == sz || !update_cache(f)) {          
            break;
        }
    }
    if (nread != 0 || sz == 0 || io61_eof(f)) 
        return nread;
    else
        return -1;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success or
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    const char* buf = (const char*) &ch;
    if (io61_write(f, buf, 1) == 1)
        return 0;
    else
        return -1;
}


// io61_write(f, buf, sz)
//    Write `sz` characters from `buf` to `f`. Returns the number of
//    characters written on success; normally this is `sz`. Returns -1 if
//    an error occurred before any characters were written.

ssize_t io61_write(io61_file* f, const char* buf, size_t sz) {
    size_t nwritten = 0;
    while (nwritten != sz) {

        // First, let's try to write to the cache
        size_t actual_written = write_cache(f->cache, buf + nwritten, sz - nwritten);

        // update how much we have written
        nwritten += actual_written;

        // If our cache isn't empty, then we need to empty the cache
        if (actual_written == 0) {
            empty_cache(f);
        }
    }
    if (nwritten != 0 || sz == 0)
        return nwritten;
    else
        return -1;
}


// io61_flush(f)
//    Forces a write of all buffered data written to `f`.
//    If `f` was opened read-only, io61_flush(f) may either drop all
//    data buffered for reading, or do nothing.

int io61_flush(io61_file* f) {
    if (f->cache->size)
        empty_cache(f);
    return 0;
}


// io61_seek(f, pos)
//    Change the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    off_t r = lseek(f->fd, (off_t) pos, SEEK_SET);
    if (r == (off_t) pos)
        return 0;
    else
        return -1;
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
//    Test if readable file `f` is at end-of-file.

int io61_eof(io61_file* f) {
    char x;
    return read(f->fd, &x, 1) == 0;
}
