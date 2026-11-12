/*
 * Minimal freestanding C runtime for the GEM6502 WebAssembly player.
 *
 * The emulator core only needs a small slice of libc: a heap, the mem/str
 * routines, and stdio entry points that are never reached in the browser.
 * Providing them here keeps the module self-contained (no WASI imports and
 * no Emscripten runtime), so index.html only needs to instantiate the .wasm.
 *
 * The heap starts at __heap_base and grows WebAssembly linear memory on
 * demand.  Blocks are kept in address order so free() can coalesce.
 */
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int errno;

/* ---------------------------------------------------------------- alloc */

typedef struct Block {
  size_t size; /* payload bytes, excluding this header */
  struct Block *next;
  int free;
  unsigned reserved;
} Block __attribute__((aligned(8)));

extern unsigned char __heap_base;

static Block *blocks;
static uintptr_t heap_top;

static uintptr_t align8(uintptr_t v) { return (v + 7u) & ~(uintptr_t)7u; }

static int ensure_memory(uintptr_t end) {
  uintptr_t have = (uintptr_t)__builtin_wasm_memory_size(0) * 65536u;
  size_t pages;

  if (end <= have) return 0;
  pages = (size_t)((end - have + 65535u) / 65536u);
  return __builtin_wasm_memory_grow(0, pages) == (size_t)-1 ? -1 : 0;
}

static Block *last_block(void) {
  Block *b = blocks;
  while (b && b->next) b = b->next;
  return b;
}

void *malloc(size_t size) {
  Block *b;

  if (size == 0) size = 1;
  size = align8(size);

  for (b = blocks; b; b = b->next) {
    if (!b->free || b->size < size) continue;
    if (b->size - size >= sizeof(Block) + 16u) {
      Block *rest = (Block *)((unsigned char *)(b + 1) + size);
      rest->size = b->size - size - sizeof(Block);
      rest->free = 1;
      rest->next = b->next;
      b->size = size;
      b->next = rest;
    }
    b->free = 0;
    return b + 1;
  }

  if (heap_top == 0) heap_top = align8((uintptr_t)&__heap_base);
  if (ensure_memory(heap_top + sizeof(Block) + size) != 0) return NULL;

  b = (Block *)heap_top;
  b->size = size;
  b->next = NULL;
  b->free = 0;
  if (blocks == NULL)
    blocks = b;
  else
    last_block()->next = b;
  heap_top += sizeof(Block) + size;
  return b + 1;
}

void free(void *ptr) {
  Block *b, *prev;

  if (ptr == NULL) return;
  b = (Block *)ptr - 1;
  b->free = 1;

  /* Coalesce forward. */
  if (b->next && b->next->free &&
      (unsigned char *)b->next == (unsigned char *)(b + 1) + b->size) {
    b->size += sizeof(Block) + b->next->size;
    b->next = b->next->next;
  }

  /* Coalesce backward: the list is address ordered. */
  for (prev = blocks; prev && prev->next; prev = prev->next) {
    if (prev->next == b && prev->free &&
        (unsigned char *)(prev + 1) + prev->size == (unsigned char *)b) {
      prev->size += sizeof(Block) + b->size;
      prev->next = b->next;
      break;
    }
  }
}

void *calloc(size_t count, size_t size) {
  size_t total;

  if (count != 0 && size > (size_t)-1 / count) return NULL;
  total = count * size;
  void *p = malloc(total);
  if (p != NULL) memset(p, 0, total);
  return p;
}

void *realloc(void *ptr, size_t size) {
  Block *b;
  void *next;

  if (ptr == NULL) return malloc(size);
  if (size == 0) {
    free(ptr);
    return NULL;
  }
  b = (Block *)ptr - 1;
  if (b->size >= size) return ptr;
  next = malloc(size);
  if (next == NULL) return NULL;
  memcpy(next, ptr, b->size);
  free(ptr);
  return next;
}

/* --------------------------------------------------------------- string */

__attribute__((optnone)) void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;
  size_t i;

  for (i = 0; i < n; i++) d[i] = s[i];
  return dest;
}

__attribute__((optnone)) void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  size_t i;

  for (i = 0; i < n; i++) p[i] = (unsigned char)c;
  return s;
}

__attribute__((optnone)) void *memmove(void *dest, const void *src, size_t n) {
  unsigned char *d = dest;
  const unsigned char *s = src;

  if (d < s) {
    size_t i;
    for (i = 0; i < n; i++) d[i] = s[i];
  } else if (d > s) {
    while (n-- > 0) d[n] = s[n];
  }
  return dest;
}

__attribute__((optnone)) int memcmp(const void *a, const void *b, size_t n) {
  const unsigned char *x = a, *y = b;
  size_t i;

  for (i = 0; i < n; i++) {
    if (x[i] != y[i]) return (int)x[i] - (int)y[i];
  }
  return 0;
}

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n]) n++;
  return n;
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
  while (n && *a && *a == *b) {
    a++;
    b++;
    n--;
  }
  if (n == 0) return 0;
  return (unsigned char)*a - (unsigned char)*b;
}

char *strncpy(char *dest, const char *src, size_t n) {
  size_t i = 0;

  for (; i < n && src[i]; i++) dest[i] = src[i];
  for (; i < n; i++) dest[i] = '\0';
  return dest;
}

char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++) != '\0') {
  }
  return dest;
}

char *strchr(const char *s, int c) {
  for (;; s++) {
    if (*s == (char)c) return (char *)s;
    if (*s == '\0') return NULL;
  }
}

char *strrchr(const char *s, int c) {
  const char *last = NULL;

  for (;; s++) {
    if (*s == (char)c) last = s;
    if (*s == '\0') return (char *)last;
  }
}

char *strstr(const char *haystack, const char *needle) {
  size_t n = strlen(needle);

  if (n == 0) return (char *)haystack;
  for (; *haystack; haystack++) {
    if (strncmp(haystack, needle, n) == 0) return (char *)haystack;
  }
  return NULL;
}

char *strerror(int errnum) {
  (void)errnum;
  return (char *)"error";
}

/* ------------------------------------------------------------- stdio */

struct GemFile {
  int unused;
};

static struct GemFile f_in, f_out, f_err;
FILE *stdin = &f_in;
FILE *stdout = &f_out;
FILE *stderr = &f_err;

FILE *fopen(const char *path, const char *mode) {
  (void)path;
  (void)mode;
  errno = ENOENT;
  return NULL;
}

size_t fread(void *ptr, size_t size, size_t count, FILE *stream) {
  (void)ptr;
  (void)size;
  (void)count;
  (void)stream;
  return 0;
}

size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream) {
  (void)ptr;
  (void)size;
  (void)count;
  (void)stream;
  return 0;
}

int fclose(FILE *stream) {
  (void)stream;
  return EOF;
}

int ferror(FILE *stream) {
  (void)stream;
  return 1;
}

int fseek(FILE *stream, long offset, int whence) {
  (void)stream;
  (void)offset;
  (void)whence;
  return -1;
}

long ftell(FILE *stream) {
  (void)stream;
  return -1;
}

int fputc(int c, FILE *stream) {
  (void)stream;
  return c;
}

int fputs(const char *str, FILE *stream) {
  (void)str;
  (void)stream;
  return EOF;
}

int putchar(int c) {
  (void)c;
  return c;
}

void perror(const char *s) {
  (void)s;
}

/* --------------------------------------------------------- formatting */

typedef struct {
  char *buf;
  size_t cap;
  size_t len;
} Out;

static void out_ch(Out *o, char c) {
  if (o->cap != 0 && o->len + 1 < o->cap) o->buf[o->len] = c;
  o->len++;
}

static void out_str(Out *o, const char *s) {
  while (*s) out_ch(o, *s++);
}

static void out_uint(Out *o, unsigned long long value, unsigned base,
                     int upper, int width, int zero_pad) {
  char tmp[32];
  int n = 0;
  int pad;
  const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

  do {
    tmp[n++] = digits[value % base];
    value /= base;
  } while (value != 0);

  pad = width - n;
  if (pad > 0 && !zero_pad) {
    while (pad-- > 0) out_ch(o, ' ');
  }
  if (pad > 0) {
    while (pad-- > 0) out_ch(o, '0');
  }
  while (n > 0) out_ch(o, tmp[--n]);
}

static int vformat(char *buf, size_t cap, const char *fmt, va_list ap) {
  Out o = {buf, cap, 0};

  while (*fmt) {
    int width = 0;
    int zero_pad = 0;
    int length = 0;
    char c = *fmt++;

    if (c != '%') {
      out_ch(&o, c);
      continue;
    }
    if (*fmt == '%') {
      out_ch(&o, '%');
      fmt++;
      continue;
    }
    if (*fmt == '0') {
      zero_pad = 1;
      fmt++;
    }
    while (*fmt >= '0' && *fmt <= '9') {
      width = width * 10 + (*fmt++ - '0');
    }
    while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
      if (*fmt == 'l') length++;
      if (*fmt == 'z') length = 3;
      fmt++;
    }

    switch (*fmt++) {
      case 's': {
        const char *s = va_arg(ap, const char *);
        out_str(&o, s ? s : "(null)");
        break;
      }
      case 'c':
        out_ch(&o, (char)va_arg(ap, int));
        break;
      case 'd':
      case 'i': {
        long long v = length >= 2 ? va_arg(ap, long long)
                      : length == 1 ? va_arg(ap, long)
                                    : va_arg(ap, int);
        unsigned long long mag =
            v < 0 ? (unsigned long long)(-(v + 1)) + 1ull : (unsigned long long)v;
        if (v < 0) out_ch(&o, '-');
        out_uint(&o, mag, 10, 0, width - (v < 0), zero_pad);
        break;
      }
      case 'u':
      case 'x':
      case 'X': {
        unsigned long long v;
        char spec = fmt[-1];
        if (length >= 2)
          v = va_arg(ap, unsigned long long);
        else if (length == 1)
          v = va_arg(ap, unsigned long);
        else if (length == 3)
          v = (unsigned long long)va_arg(ap, size_t);
        else
          v = va_arg(ap, unsigned int);
        out_uint(&o, v, spec == 'u' ? 10 : 16, spec == 'X', width, zero_pad);
        break;
      }
      default:
        out_ch(&o, '%');
        out_ch(&o, fmt[-1]);
        break;
    }
  }

  if (cap != 0) {
    size_t end = o.len < cap - 1 ? o.len : cap - 1;
    buf[end] = '\0';
  }
  return (int)o.len;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
  return vformat(str, size, format, ap);
}

int snprintf(char *str, size_t size, const char *format, ...) {
  int rc;
  va_list ap;

  va_start(ap, format);
  rc = vformat(str, size, format, ap);
  va_end(ap);
  return rc;
}

int fprintf(FILE *stream, const char *format, ...) {
  char scratch[256];
  int rc;
  va_list ap;

  (void)stream;
  va_start(ap, format);
  rc = vformat(scratch, sizeof(scratch), format, ap);
  va_end(ap);
  return rc;
}

int printf(const char *format, ...) {
  char scratch[256];
  int rc;
  va_list ap;

  va_start(ap, format);
  rc = vformat(scratch, sizeof(scratch), format, ap);
  va_end(ap);
  return rc;
}

/* -------------------------------------------------------------- misc */

char *getenv(const char *name) {
  (void)name;
  return NULL;
}

void exit(int status) {
  (void)status;
  __builtin_trap();
}

void abort(void) { __builtin_trap(); }
