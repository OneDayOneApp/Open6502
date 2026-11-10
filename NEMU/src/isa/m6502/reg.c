#include <strings.h>
#include <isa.h>
#include "local-include/m6502.h"

void isa_reg_display(void) {
  printf("%s A:%02X X:%02X Y:%02X SP:%02X P:%02X [%c%c%c%c%c%c%c%c] PC:%04X CYC:%" PRIu64 "\n",
      cpu.is_65c02 ? "65C02" : "6502",
      cpu.a, cpu.x, cpu.y, cpu.sp, cpu.p,
      (cpu.p & M6502_N) ? 'N' : '.',
      (cpu.p & M6502_V) ? 'V' : '.',
      (cpu.p & M6502_U) ? 'U' : '.',
      (cpu.p & M6502_B) ? 'B' : '.',
      (cpu.p & M6502_D) ? 'D' : '.',
      (cpu.p & M6502_I) ? 'I' : '.',
      (cpu.p & M6502_Z) ? 'Z' : '.',
      (cpu.p & M6502_C) ? 'C' : '.',
      (unsigned)(cpu.pc & 0xFFFF), cpu.cycles);
}

word_t isa_reg_str2val(const char *s, bool *success) {
  if (s == NULL) { *success = false; return 0; }
  if (*s == '$') s++;
  *success = true;
  if (strcasecmp(s, "a") == 0) return cpu.a;
  if (strcasecmp(s, "x") == 0) return cpu.x;
  if (strcasecmp(s, "y") == 0) return cpu.y;
  if (strcasecmp(s, "sp") == 0) return cpu.sp;
  if (strcasecmp(s, "p") == 0 || strcasecmp(s, "ps") == 0) return cpu.p;
  if (strcasecmp(s, "pc") == 0) return cpu.pc & 0xFFFF;
  if (strcasecmp(s, "cycles") == 0 || strcasecmp(s, "cyc") == 0) return cpu.cycles;
  *success = false;
  return 0;
}
