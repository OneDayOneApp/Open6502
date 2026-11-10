#include <isa.h>
#include <memory/paddr.h>
#include <memory/vaddr.h>
#include "local-include/m6502.h"

/*
 * Built-in image.  It prints a greeting through the emulator's host I/O port
 * ($F001) and then exits with code 0 through $F002.  A real image passed on
 * the command line replaces this program.
 */
static const uint8_t builtin_img[] = {
  0xA2, 0x00,             /* 8000: LDX #$00             */
  0xBD, 0x16, 0x80,       /* 8002: LDA $8016,X          */
  0xF0, 0x07,             /* 8005: BEQ $800E            */
  0x8D, 0x01, 0xF0,       /* 8007: STA $F001            */
  0xE8,                   /* 800A: INX                  */
  0x4C, 0x02, 0x80,       /* 800B: JMP $8002            */
  0xA9, 0x00,             /* 800E: LDA #$00             */
  0x8D, 0x02, 0xF0,       /* 8010: STA $F002            */
  0x4C, 0x13, 0x80,       /* 8013: JMP $8013            */
  'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r', 'o', 'm', ' ', '6', '5', '0', '2',
  'E', 'M', 'U', '!', '\n', 0x00,
};

uint16_t m6502_read16(uint16_t addr) {
  return vaddr_read(addr, 1) | ((uint16_t)vaddr_read((uint16_t)(addr + 1), 1) << 8);
}

static void m6502_power_on(void) {
  cpu.a = 0;
  cpu.x = 0;
  cpu.y = 0;
  cpu.sp = 0xFD;
  cpu.p = M6502_U | M6502_I;
  cpu.pc = RESET_VECTOR;
  cpu.cycles = 0;
  cpu.irq_pending = false;
  cpu.nmi_pending = false;
}

void init_isa(void) {
  memcpy(guest_to_host(RESET_VECTOR), builtin_img, sizeof(builtin_img));
  m6502_power_on();
  cpu.is_65c02 = false;
}

/*
 * Called by the monitor after a user image has been loaded.  The default entry
 * is the load address, which makes raw 6502 binaries directly runnable.  Use
 * --pc to select another entry point, or --reset to use the 6502 reset vector
 * at $FFFC (useful for ROM images that contain their own vectors).
 */
void isa_post_load(long img_size, paddr_t load_addr, vaddr_t entry, bool use_reset) {
  (void)img_size;
  if (use_reset) {
    cpu.pc = m6502_read16(0xFFFC);
  } else if (entry != (vaddr_t)-1) {
    cpu.pc = entry;
  } else {
    cpu.pc = load_addr;
  }
  cpu.pc &= 0xFFFF;
}

void m6502_reset(void) {
  m6502_power_on();
  cpu.pc = m6502_read16(0xFFFC);
}

void m6502_request_irq(void) { cpu.irq_pending = true; }
void m6502_request_nmi(void) { cpu.nmi_pending = true; }
