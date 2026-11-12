#ifndef GEM_WEB_STDLIB_H
#define GEM_WEB_STDLIB_H

/* Minimal <stdlib.h> for the freestanding WebAssembly build. */

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

char *getenv(const char *name);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));

#endif
