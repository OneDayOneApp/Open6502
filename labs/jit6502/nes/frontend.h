/*
 * Optional SDL2 window for the NES lab.
 *
 * The engine is supplied by the caller as a "advance one frame" callback, so
 * the same window can drive the interpreter or the JIT.
 */
#ifndef J6502_NES_FRONTEND_H
#define J6502_NES_FRONTEND_H

#include "cpu.h"

typedef int (*NesFrameStep)(Cpu *c);

/* Returns 1 when a window was opened and the user quit normally, 0 on error. */
int nes_run_window(Cpu *c, int scale, NesFrameStep step);

/* Whether this build has SDL2 support. */
int nes_window_available(void);

#endif /* J6502_NES_FRONTEND_H */
