/*
 * am6502 — the contract between an app and a 6502 machine.
 *
 * This header is the 6502 subset of nexus-am's `am.h`: TRM (putc/halt/heap)
 * and a NES-shaped IOE only.  CTE/VME/MPE are deliberately absent; see
 * ../../docs/design.md §3 for why.
 *
 * A C compiler for this header is not wired up yet (M2).  The assembly
 * runtime in am/src/nemu/ implements the same symbols by hand, so an app
 * written against this header can be ported without changing its calls.
 */
#ifndef __AM_H__
#define __AM_H__

#include <stdint.h>
#include <stddef.h>

#include "amdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Memory area for [@start, @end).  On `mos6502-nemu` the heap is whatever the
 * linker left of the platform's RAM area; see am/src/nemu/flat.cfg. */
typedef struct _Area {
  void *start, *end;
} _Area;

extern _Area _heap;

/* TRM ---------------------------------------------------------------- */

/* Write one character to the host console: $F001 on mos6502-nemu, $4018 on
 * mos6502-nes.  On real hardware both are inert, so an app that must show
 * output on a cartridge writes to the PPU instead. */
void _putc(char ch);

/* Stop the machine with @code (low 7 bits) as the exit status, then spin.
 * Never returns, on hardware or in simulation. */
void _halt(int code) __attribute__((__noreturn__));

/* IOE ---------------------------------------------------------------- */

/* Device-register access, in AM's shape.  Unimplemented (dev, reg) pairs
 * return 0 rather than trapping: one app must be buildable for every
 * platform.  `dev` values are in amdev.h. */
size_t _io_read(uint32_t dev, uintptr_t reg, void *buf, size_t size);
size_t _io_write(uint32_t dev, uintptr_t reg, const void *buf, size_t size);

/* Build-time argument string (`make ... mainargs='m2'`).  Never NULL; ""
 * when unset.  There is no argc/argv: 2 KiB of WRAM cannot afford AM's
 * stack-built argv, and no M1 app needs it. */
const char *am_mainargs(void);

#ifdef __cplusplus
}
#endif

#endif /* __AM_H__ */
