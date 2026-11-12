#ifndef GEM_WEB_ERRNO_H
#define GEM_WEB_ERRNO_H

/* Minimal <errno.h> for the freestanding WebAssembly build. */

extern int errno;

#define ENOENT 2
#define EIO 5
#define ENOMEM 12
#define EINVAL 22

#endif
