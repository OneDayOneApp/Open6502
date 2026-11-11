#ifndef GEM_KERNEL_H
#define GEM_KERNEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t GemTick;
typedef uint64_t GemEventHandle;
typedef struct {
  GemTick tick;
  uint32_t delta;
  uint16_t phase;
  uint64_t sequence;
} GemTime;
typedef struct {
  GemTime at;
  GemEventHandle handle;
  uint32_t owner, kind;
  uint64_t payload;
} GemScheduledEvent;
typedef enum {
  GEM_SIM_OK, GEM_SIM_INVALID, GEM_SIM_PAST, GEM_SIM_ORDER,
  GEM_SIM_OVERFLOW, GEM_SIM_NOMEM, GEM_SIM_DELTA_LIMIT,
  GEM_SIM_NOT_FOUND, GEM_SIM_REENTRANT
} GemSimResult;
typedef struct GemKernel GemKernel;
typedef void (*GemEventHandler)(GemKernel *, const GemScheduledEvent *, void *);
typedef struct { GemEventHandler handler; void *context; } GemEventOwner;
struct GemKernel {
  GemTime now;
  GemScheduledEvent *events;
  size_t count, capacity;
  uint64_t next_handle, next_sequence;
  uint32_t max_delta;
  bool running, closed;
  GemEventOwner owners[32];
};
/* Callbacks belong to an instance registry, not to serialized event records.
 * Event storage is private to the kernel; callers must use this API. */
void gem_kernel_init(GemKernel *, uint32_t max_delta);
void gem_kernel_destroy(GemKernel *);
GemSimResult gem_kernel_bind(GemKernel *, uint32_t owner, GemEventHandler, void *);
GemSimResult gem_kernel_reset(GemKernel *);
GemSimResult gem_kernel_schedule(GemKernel *, GemTime, uint32_t owner,
                                uint32_t kind, uint64_t payload, GemEventHandle *);
GemSimResult gem_kernel_cancel(GemKernel *, GemEventHandle);
GemSimResult gem_kernel_reschedule(GemKernel *, GemEventHandle, GemTime);
/* Inclusive boundary: all events at tick <= until are processed. Successful
 * return closes that tick; new work must be scheduled after it. */
GemSimResult gem_kernel_run_until(GemKernel *, GemTick until);
typedef struct { GemTick period, phase; } GemClock;
GemSimResult gem_clock_edge_at_or_after(GemClock, GemTick, GemTick *);
#endif
