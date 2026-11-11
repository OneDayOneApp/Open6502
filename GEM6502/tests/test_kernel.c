#include "gem/kernel.h"
#include <assert.h>
#include <stdio.h>

typedef struct { unsigned count; uint64_t values[64]; GemSimResult rejected; } Log;
static void handler(GemKernel *k, const GemScheduledEvent *e, void *arg) {
  Log *log = arg;
  assert(k->now.tick == e->at.tick && k->now.phase == e->at.phase);
  log->values[log->count++] = e->payload;
  if (e->kind == 1) {
    GemEventHandle handle;
    GemTime at = e->at;
    assert(gem_kernel_run_until(k, at.tick) == GEM_SIM_REENTRANT);
    assert(gem_kernel_reset(k) == GEM_SIM_REENTRANT);
    log->rejected = gem_kernel_schedule(k, at, 0, 0, 99, &handle);
    at.delta++;
    at.phase = 10;
    assert(gem_kernel_schedule(k, at, 0, 0, 8, &handle) == GEM_SIM_OK);
  }
  if (e->kind == 2) {
    GemEventHandle handle;
    GemTime at = e->at;
    at.delta++;
    log->rejected = gem_kernel_schedule(k, at, 0, 2, 0, &handle);
  }
}
static GemEventHandle add(GemKernel *k, uint64_t tick, uint16_t phase, uint32_t kind, uint64_t value) {
  GemEventHandle handle;
  assert(gem_kernel_schedule(k, (GemTime){.tick=tick,.phase=phase}, 0, kind, value, &handle) == GEM_SIM_OK);
  return handle;
}
int main(void) {
  GemKernel k, other;
  Log log = {0}, second = {0};
  gem_kernel_init(&k, 3); gem_kernel_init(&other, 3);
  assert(gem_kernel_bind(&k, 0, handler, &log) == GEM_SIM_OK);
  assert(gem_kernel_bind(&other, 0, handler, &second) == GEM_SIM_OK);
  add(&k, 5, 20, 0, 2); add(&k, 5, 10, 0, 1);
  add(&k, 5, 20, 0, 3);
  GemEventHandle cancelled=add(&k, 4, 10, 0, 99);
  assert(gem_kernel_cancel(&k, cancelled) == GEM_SIM_OK);
  assert(gem_kernel_cancel(&k, cancelled) == GEM_SIM_NOT_FOUND);
  GemEventHandle moved=add(&k, 8, 10, 0, 4);
  assert(gem_kernel_reschedule(&k, moved, (GemTime){.tick=6,.phase=10}) == GEM_SIM_OK);
  assert(gem_kernel_run_until(&k, 5) == GEM_SIM_OK);
  assert(log.count == 3 && log.values[0] == 1 && log.values[1] == 2 && log.values[2] == 3);
  assert(k.count == 1 && k.now.tick == 5 && other.now.tick == 0);
  GemEventHandle invalid;
  assert(gem_kernel_schedule(&k, (GemTime){.tick=5,.phase=90},0,0,0,&invalid) == GEM_SIM_PAST);
  assert(gem_kernel_reschedule(&k, moved, (GemTime){.tick=4}) == GEM_SIM_PAST);
  assert(k.count == 1); /* Rejected reschedule must preserve pending work. */
  assert(gem_kernel_run_until(&k, 6) == GEM_SIM_OK && log.values[3] == 4);
  add(&k, 7, 20, 1, 7);
  assert(gem_kernel_run_until(&k, 7) == GEM_SIM_OK);
  assert(log.rejected == GEM_SIM_ORDER && log.values[5] == 8);
  add(&k, 8, 10, 2, 0);
  assert(gem_kernel_run_until(&k, 8) == GEM_SIM_OK);
  assert(log.rejected == GEM_SIM_DELTA_LIMIT);
  GemEventHandle old=add(&k, 100, 10, 0, 77);
  assert(gem_kernel_reset(&k) == GEM_SIM_OK && k.count == 0 && k.now.tick == 0);
  GemEventHandle fresh=add(&k, 1, 10, 0, 55);
  assert(old != fresh && gem_kernel_cancel(&k, old) == GEM_SIM_NOT_FOUND);
  assert(gem_kernel_run_until(&k, 1) == GEM_SIM_OK && log.values[log.count-1] == 55);
  assert(gem_kernel_run_until(&k, 0) == GEM_SIM_PAST);
  /* Independent periodic clocks: periods 12 and 4 in the same time base. */
  assert(gem_kernel_reset(&k) == GEM_SIM_OK);
  log.count=0;
  for (GemTick t=0;t<=24;t+=12) add(&k,t,10,0,12);
  for (GemTick t=0;t<=24;t+=4) add(&k,t,20,0,4);
  assert(gem_kernel_run_until(&k,24) == GEM_SIM_OK && log.count == 10);
  assert(log.values[0] == 12 && log.values[1] == 4 && log.values[9] == 4);
  add(&other, 2, 10, 0, 88);
  assert(gem_kernel_run_until(&other, 2) == GEM_SIM_OK && second.values[0] == 88);
  GemTick edge;
  assert(gem_clock_edge_at_or_after((GemClock){12,3},4,&edge)==GEM_SIM_OK && edge==15);
  assert(gem_clock_edge_at_or_after((GemClock){12,3},15,&edge)==GEM_SIM_OK && edge==15);
  assert(gem_clock_edge_at_or_after((GemClock){0,0},0,&edge)==GEM_SIM_INVALID);
  assert(gem_clock_edge_at_or_after((GemClock){4,0},UINT64_MAX,&edge)==GEM_SIM_OVERFLOW);
  k.next_sequence=UINT64_MAX;
  assert(gem_kernel_schedule(&k,(GemTime){.tick=30},0,0,0,&invalid)==GEM_SIM_OVERFLOW);
  gem_kernel_destroy(&k); gem_kernel_destroy(&other);
  puts("PASS: experimental kernel ordering, cancellation, reschedule, delta limit, boundaries, clocks, reset and instance isolation");
}
