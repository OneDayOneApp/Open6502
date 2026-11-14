/* Unit check for src/arm64.h against encodings produced by llvm-mc.
 * Build/run: cc -I src -o /tmp/enccheck tools/enccheck.c && /tmp/enccheck */
#include <stdio.h>
#include "arm64.h"

static int fails;

static void chk(const char *what, uint32_t got, uint32_t want) {
  if (got != want) {
    printf("FAIL %-32s got %08x want %08x\n", what, got, want);
    fails++;
  }
}

int main(void) {
  chk("movz w0,#42", a64_movz(false, 0, 42, 0), 0x52800540u);
  chk("stp x29,x30,[sp,#-16]!", a64_stp_pre(29, 30, 31, -16), 0xA9BF7BFDu);
  chk("stp x19,x20,[sp,#-16]!", a64_stp_pre(19, 20, 31, -16), 0xA9BF53F3u);
  chk("ldp x29,x30,[sp],#16", a64_ldp_post(29, 30, 31, 16), 0xA8C17BFDu);
  chk("ldp x27,x28,[sp],#16", a64_ldp_post(27, 28, 31, 16), 0xA8C173FBu);
  chk("ldrb w1,[x0,x1]", a64_ldrb_reg(1, 0, 1), 0x38616801u);
  chk("strb w1,[x0,x1]", a64_strb_reg(1, 0, 1), 0x38216801u);
  chk("ldrb w2,[x0,#12]", a64_ldrb_off(2, 0, 12), 0x39403002u);
  chk("strh w0,[x19,#6]", a64_strh_off(0, 19, 6), 0x79000E60u);
  chk("ldrh w0,[x19,#6]", a64_ldrh_off(0, 19, 6), 0x79400E60u);
  chk("str x27,[x19,#8]", a64_str_off(27, 19, 8), 0xF900067Bu);
  chk("ldr x27,[x19,#8]", a64_ldr_off(27, 19, 8), 0xF940067Bu);
  chk("add w1,w2,#255", a64_addsub_imm(false, 1, 2, 255, false), 0x1103FC41u);
  chk("sub w1,w2,#1", a64_addsub_imm(false, 1, 2, 1, true), 0x51000441u);
  chk("cmp w1,#1024", a64_cmp_imm(false, 1, 1024), 0x7110003Fu);
  chk("cset w3,eq", a64_cset(3, A64_EQ), 0x1A9F17E3u);
  chk("and w10,w9,#0x80", a64_logic_imm(0, 10, 9, 0x80), 0x1219012Au);
  chk("and w25,w25,#0xfffffffe", a64_logic_imm(0, 25, 25, 0xFFFFFFFEu), 0x121F7B39u);
  chk("and w25,w25,#0xffffffbf", a64_logic_imm(0, 25, 25, 0xFFFFFFBFu), 0x12197B39u);
  chk("and w25,w25,#0x7f", a64_logic_imm(0, 25, 25, 0x7F), 0x12001B39u);
  chk("orr w25,w25,#2", a64_logic_imm(1, 25, 25, 2), 0x321F0339u);
  chk("ands w2,w3,#0xff", a64_logic_imm(3, 2, 3, 0xFF), 0x72001C62u);
  chk("tbz w25,#1,#8", a64_tbz(25, 1, 8, false), 0x36080059u);
  chk("cbz w0,#8", a64_cbz(0, 8, false), 0x34000040u);
  chk("b.eq #8", a64_bcond(A64_EQ, 8), 0x54000040u);
  chk("ubfx w25,w0,#8,#8", a64_ubfm(false, 25, 0, 8, 15), 0x53083C19u);
  chk("lsl w1,w1,#1", a64_lsl_imm(false, 1, 1, 1), 0x531F7821u);
  chk("lsr w1,w1,#1", a64_lsr_imm(false, 1, 1, 1), 0x53017C21u);
  chk("lsr w6,w3,#8", a64_lsr_imm(false, 6, 3, 8), 0x53087C66u);
  chk("uxtb w1,w1", a64_uxtb(1, 1), 0x53001C21u);
  chk("orr w25,w25,w2,lsl#1", a64_logic_reg(false, 1, 25, 25, 2, A64_LSL, 1), 0x2A020739u);
  chk("orr w25,w25,w4,lsl#6", a64_logic_reg(false, 1, 25, 25, 4, A64_LSL, 6), 0x2A041B39u);
  chk("eor w21,w21,w9", a64_logic_reg(false, 2, 21, 21, 9, A64_LSL, 0), 0x4A0902B5u);
  chk("add x26,x20,x0", a64_addsub_reg(true, 26, 20, 0, A64_LSL, 0, false), 0x8B00029Au);
  chk("sub x0,x26,x20", a64_addsub_reg(true, 0, 26, 20, A64_LSL, 0, true), 0xCB140340u);
  chk("add w3,w21,w9", a64_addsub_reg(false, 3, 21, 9, A64_LSL, 0, false), 0x0B0902A3u);
  chk("add w3,w3,w2", a64_addsub_reg(false, 3, 3, 2, A64_LSL, 0, false), 0x0B020063u);
  chk("sub w10,w21,w9", a64_addsub_reg(false, 10, 21, 9, A64_LSL, 0, true), 0x4B0902AAu);
  chk("mov x19,x0", a64_mov_reg(true, 19, 0), 0xAA0003F3u);
  chk("mvn w9,w9", a64_mvn(9, 9), 0x2A2903E9u);
  chk("bic w4,w4,w5", a64_bic_reg(4, 4, 5), 0x0A250084u);
  chk("movz w12,#0xfe", a64_movz(false, 12, 0xFE, 0), 0x52801FCCu);
  chk("ret", a64_ret(), 0xD65F03C0u);
  if (a64_encode_bitmask(0x7D, &(uint32_t){0})) { printf("FAIL 0x7d accepted\n"); fails++; }
  printf("%s (%d failure%s)\n", fails ? "FAILED" : "OK", fails, fails == 1 ? "" : "s");
  return fails != 0;
}
