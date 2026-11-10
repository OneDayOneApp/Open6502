#ifndef __ISA_M6502_H__
#define __ISA_M6502_H__

#include <common.h>

/* NMOS 6502 processor status register bits. */
enum {
  M6502_C = 0x01,
  M6502_Z = 0x02,
  M6502_I = 0x04,
  M6502_D = 0x08,
  M6502_B = 0x10,
  M6502_U = 0x20,
  M6502_V = 0x40,
  M6502_N = 0x80,
};

/* Addressing modes, also used by the disassembler. */
typedef enum {
  AM_IMP, AM_ACC, AM_IMM, AM_ZP, AM_ZPX, AM_ZPY,
  AM_ABS, AM_ABSX, AM_ABSY, AM_IND, AM_INDX, AM_INDY,
  AM_REL, AM_ZPIND, AM_ABSXIND, AM_ZPREL, AM_ILL,
} m6502_addrmode;

enum {
  OP_ADC,
  OP_AND,
  OP_ASL,
  OP_BCC,
  OP_BCS,
  OP_BEQ,
  OP_BIT,
  OP_BMI,
  OP_BNE,
  OP_BPL,
  OP_BRK,
  OP_BVC,
  OP_BVS,
  OP_CLC,
  OP_CLD,
  OP_CLI,
  OP_CLV,
  OP_CMP,
  OP_CPX,
  OP_CPY,
  OP_DEC,
  OP_DEX,
  OP_DEY,
  OP_EOR,
  OP_INC,
  OP_INX,
  OP_INY,
  OP_JMP,
  OP_JSR,
  OP_LDA,
  OP_LDX,
  OP_LDY,
  OP_LSR,
  OP_ORA,
  OP_PHA,
  OP_PHP,
  OP_PLA,
  OP_PLP,
  OP_ROL,
  OP_ROR,
  OP_RTI,
  OP_RTS,
  OP_SBC,
  OP_SEC,
  OP_SED,
  OP_SEI,
  OP_STA,
  OP_STX,
  OP_STY,
  OP_TAX,
  OP_TAY,
  OP_TSX,
  OP_TXA,
  OP_TXS,
  OP_TYA,
  OP_BRA,
  OP_PHX,
  OP_PHY,
  OP_PLX,
  OP_PLY,
  OP_STZ,
  OP_TSB,
  OP_TRB,
  OP_RMB,
  OP_SMB,
  OP_BBR,
  OP_BBS,
  OP_WAI,
  OP_STP,
  OP_ILL,
  OP_NOP,
  OP_TRAP,
  OP_COUNT
};

typedef struct {
  const char *mnemonic;
  uint8_t mode;
  uint8_t cycles;
  uint8_t op;
} m6502_opcode_info;

extern const m6502_opcode_info m6502_opcode_table[256];

int m6502_inst_length(uint8_t opcode);
bool m6502_is_65c02(uint8_t opcode);
bool m6502_is_legacy_nop(uint8_t opcode);
uint16_t m6502_read16(uint16_t addr);
void m6502_reset(void);
void m6502_request_irq(void);
void m6502_request_nmi(void);

#endif
