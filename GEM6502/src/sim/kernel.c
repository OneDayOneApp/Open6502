#include "gem/kernel.h"
#include <stdlib.h>
#include <string.h>

static int compare(GemTime a, GemTime b) {
#define FIELD(f) if (a.f != b.f) return a.f < b.f ? -1 : 1
  FIELD(tick); FIELD(delta); FIELD(phase); FIELD(sequence);
#undef FIELD
  return 0;
}
void gem_kernel_init(GemKernel *k, uint32_t max_delta) {
  memset(k, 0, sizeof(*k));
  k->max_delta = max_delta;
  k->next_handle = 1;
}
void gem_kernel_destroy(GemKernel *k) {
  free(k->events);
  memset(k, 0, sizeof(*k));
}
GemSimResult gem_kernel_bind(GemKernel *k, uint32_t owner, GemEventHandler handler, void *context) {
  if (k->running) return GEM_SIM_REENTRANT;
  if (owner >= 32 || !handler || k->owners[owner].handler) return GEM_SIM_INVALID;
  k->owners[owner] = (GemEventOwner){handler, context};
  return GEM_SIM_OK;
}
GemSimResult gem_kernel_reset(GemKernel *k) {
  if (k->running) return GEM_SIM_REENTRANT;
  k->count = 0;
  k->now = (GemTime){0};
  k->next_sequence = 0;
  k->closed = false;
  /* Handles never repeat across reset, so old handles cannot cancel new work. */
  return GEM_SIM_OK;
}
static GemSimResult validate(const GemKernel *k, GemTime at) {
  if (at.delta > k->max_delta) return GEM_SIM_DELTA_LIMIT;
  if (at.tick < k->now.tick || (k->closed && at.tick == k->now.tick)) return GEM_SIM_PAST;
  if (k->running && at.tick == k->now.tick &&
      (at.delta < k->now.delta || (at.delta == k->now.delta && at.phase <= k->now.phase)))
    return GEM_SIM_ORDER;
  return GEM_SIM_OK;
}
/* Sorted queue for the initial correctness slice; O(n) insertion/removal.
 * A heap may replace it only after normalized event equivalence tests. */
static void insert(GemKernel *k, GemScheduledEvent event) {
  size_t i = k->count;
  while (i && compare(event.at, k->events[i-1].at) < 0) {
    k->events[i] = k->events[i-1];
    i--;
  }
  k->events[i] = event;
  k->count++;
}
GemSimResult gem_kernel_schedule(GemKernel *k, GemTime at, uint32_t owner,
                                uint32_t kind, uint64_t payload, GemEventHandle *handle) {
  if (owner >= 32 || !k->owners[owner].handler || !handle) return GEM_SIM_INVALID;
  GemSimResult result = validate(k, at);
  if (result != GEM_SIM_OK) return result;
  if (k->next_handle == UINT64_MAX || k->next_sequence == UINT64_MAX) return GEM_SIM_OVERFLOW;
  if (k->count == k->capacity) {
    if (k->capacity > SIZE_MAX / 2 / sizeof(*k->events)) return GEM_SIM_OVERFLOW;
    size_t capacity = k->capacity ? k->capacity * 2 : 16;
    GemScheduledEvent *events = realloc(k->events, capacity * sizeof(*events));
    if (!events) return GEM_SIM_NOMEM;
    k->events = events;
    k->capacity = capacity;
  }
  at.sequence = k->next_sequence++;
  *handle = k->next_handle++;
  insert(k, (GemScheduledEvent){at, *handle, owner, kind, payload});
  return GEM_SIM_OK;
}
static size_t find(const GemKernel *k, GemEventHandle handle) {
  for (size_t i = 0; i < k->count; i++) if (k->events[i].handle == handle) return i;
  return k->count;
}
static GemScheduledEvent remove_at(GemKernel *k, size_t i) {
  GemScheduledEvent result = k->events[i];
  memmove(k->events + i, k->events + i + 1, (k->count - i - 1) * sizeof(*k->events));
  k->count--;
  return result;
}
GemSimResult gem_kernel_cancel(GemKernel *k, GemEventHandle handle) {
  size_t i = find(k, handle);
  if (i == k->count) return GEM_SIM_NOT_FOUND;
  remove_at(k, i);
  return GEM_SIM_OK;
}
GemSimResult gem_kernel_reschedule(GemKernel *k, GemEventHandle handle, GemTime at) {
  size_t i = find(k, handle);
  if (i == k->count) return GEM_SIM_NOT_FOUND;
  GemSimResult result = validate(k, at);
  if (result != GEM_SIM_OK) return result;
  if (k->next_sequence == UINT64_MAX) return GEM_SIM_OVERFLOW;
  GemScheduledEvent event = remove_at(k, i);
  at.sequence = k->next_sequence++;
  event.at = at;
  insert(k, event);
  return GEM_SIM_OK;
}
GemSimResult gem_kernel_run_until(GemKernel *k, GemTick until) {
  if (k->running) return GEM_SIM_REENTRANT;
  if (until < k->now.tick) return GEM_SIM_PAST;
  k->running = true;
  while (k->count && k->events[0].at.tick <= until) {
    GemScheduledEvent event = remove_at(k, 0);
    k->closed = false;
    k->now = event.at; /* Publish time before invoking the handler. */
    k->owners[event.owner].handler(k, &event, k->owners[event.owner].context);
  }
  k->now = (GemTime){.tick = until};
  k->closed = true;
  k->running = false;
  return GEM_SIM_OK;
}
GemSimResult gem_clock_edge_at_or_after(GemClock clock, GemTick tick, GemTick *out) {
  if (!out || !clock.period || clock.phase >= clock.period) return GEM_SIM_INVALID;
  if (tick <= clock.phase) { *out = clock.phase; return GEM_SIM_OK; }
  GemTick remainder = (tick - clock.phase) % clock.period;
  GemTick offset = remainder ? clock.period - remainder : 0;
  if (tick > UINT64_MAX - offset) return GEM_SIM_OVERFLOW;
  *out = tick + offset;
  return GEM_SIM_OK;
}
