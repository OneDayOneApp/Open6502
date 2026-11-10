#include <common.h>
#include <cpu/decode.h>
#include "local-include/m6502.h"

void init_disasm(void) {
  /* 6502 disassembly is built into this ISA port; no host library is needed. */
}

void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte) {
  if (nbyte <= 0 || size <= 0) {
    if (size > 0) str[0] = '\0';
    return;
  }

  uint8_t opcode = code[0];
  const m6502_opcode_info *info = &m6502_opcode_table[opcode];
  if (info->mode == AM_ILL) {
    snprintf(str, size, ".byte $%02X", opcode);
    return;
  }

  const char *mnemonic = info->mnemonic;
  switch (info->mode) {
    case AM_IMP:
      snprintf(str, size, "%s", mnemonic);
      break;
    case AM_ACC:
      snprintf(str, size, "%s A", mnemonic);
      break;
    case AM_IMM:
      snprintf(str, size, "%s #$%02X", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_ZP:
      snprintf(str, size, "%s $%02X", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_ZPX:
      snprintf(str, size, "%s $%02X,X", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_ZPY:
      snprintf(str, size, "%s $%02X,Y", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_ABS:
      snprintf(str, size, "%s $%04X", mnemonic,
          nbyte > 2 ? (unsigned)(code[1] | (code[2] << 8)) : 0u);
      break;
    case AM_ABSX:
      snprintf(str, size, "%s $%04X,X", mnemonic,
          nbyte > 2 ? (unsigned)(code[1] | (code[2] << 8)) : 0u);
      break;
    case AM_ABSY:
      snprintf(str, size, "%s $%04X,Y", mnemonic,
          nbyte > 2 ? (unsigned)(code[1] | (code[2] << 8)) : 0u);
      break;
    case AM_IND:
      snprintf(str, size, "%s ($%04X)", mnemonic,
          nbyte > 2 ? (unsigned)(code[1] | (code[2] << 8)) : 0u);
      break;
    case AM_INDX:
      snprintf(str, size, "%s ($%02X,X)", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_INDY:
      snprintf(str, size, "%s ($%02X),Y", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_REL: {
      int8_t off = (int8_t)(nbyte > 1 ? code[1] : 0);
      uint16_t target = (uint16_t)(pc + 2 + off);
      snprintf(str, size, "%s $%04X", mnemonic, target);
      break;
    }
    case AM_ZPIND:
      snprintf(str, size, "%s ($%02X)", mnemonic, nbyte > 1 ? code[1] : 0);
      break;
    case AM_ABSXIND:
      snprintf(str, size, "%s ($%04X,X)", mnemonic,
          nbyte > 2 ? (unsigned)(code[1] | (code[2] << 8)) : 0u);
      break;
    case AM_ZPREL: {
      int8_t off = (int8_t)(nbyte > 2 ? code[2] : 0);
      uint16_t target = (uint16_t)(pc + 3 + off);
      snprintf(str, size, "%s $%02X,$%04X", mnemonic,
          nbyte > 1 ? code[1] : 0, target);
      break;
    }
    default:
      snprintf(str, size, ".byte $%02X", opcode);
      break;
  }
}
