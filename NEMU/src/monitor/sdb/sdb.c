/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <isa.h>
#include <cpu/cpu.h>
#include <memory/paddr.h>
#include <memory/vaddr.h>
#include <readline/readline.h>
#include <readline/history.h>
#ifdef CONFIG_ISA_m6502
#include <m6502.h>
#endif
#include "sdb.h"

static bool is_batch_mode = false;
uint64_t monitor_max_inst(void);
void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);

void init_regex(void);
void init_wp_pool(void);

#define MAX_XDUMP 4096

static uint64_t parse_count(const char *s, uint64_t fallback) {
  if (s == NULL || *s == '\0') return fallback;
  char *end = NULL;
  errno = 0;
  unsigned long long v = strtoull(s, &end, 0);
  if (errno != 0 || end == s || *end != '\0') {
    printf("invalid number: '%s'\n", s);
    return fallback;
  }
  return (uint64_t)v;
}

static bool parse_addr(const char *s, uint32_t *out) {
  if (s == NULL || *s == '\0') return false;
  if (*s == '$') s++;
  char *end = NULL;
  errno = 0;
  /* Monitor addresses and register values are hexadecimal by convention. */
  unsigned long v = strtoul(s, &end, 16);
  if (errno != 0 || end == s || *end != '\0') return false;
  *out = (uint32_t)v;
  return true;
}

static char *next_arg(char **saveptr) {
  if (saveptr == NULL || *saveptr == NULL) return NULL;
  char *p = *saveptr;
  while (*p != '\0' && isspace((unsigned char)*p)) p++;
  if (*p == '\0') { *saveptr = NULL; return NULL; }
  char *start = p;
  while (*p != '\0' && !isspace((unsigned char)*p)) p++;
  if (*p != '\0') { *p = '\0'; *saveptr = p + 1; }
  else *saveptr = NULL;
  return start;
}

static char *rl_gets(void) {
  static char *line_read = NULL;
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }
  line_read = readline("(6502EMU) ");
  if (line_read && *line_read) add_history(line_read);
  return line_read;
}

static int cmd_help(char *args);

static int cmd_c(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_d(char *args);
static int cmd_set(char *args);
static int cmd_load(char *args);
static int cmd_reset(char *args);
static int cmd_irq(char *args);
static int cmd_nmi(char *args);
static int cmd_q(char *args);

static const struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
  { "help", "Display information about supported commands", cmd_help },
  { "c",    "Continue: c [N] (run N instructions, default all)", cmd_c },
  { "run",  "Alias of c", cmd_c },
  { "si",   "Step: si [N] (default 1)", cmd_si },
  { "info", "info r - display 6502 registers", cmd_info },
  { "x",    "Examine memory: x ADDR [LEN]", cmd_x },
  { "d",    "Disassemble: d [ADDR [COUNT]]", cmd_d },
  { "set",  "Set register: set REG VALUE", cmd_set },
  { "load", "Load raw binary: load FILE [ADDR]", cmd_load },
  { "reset", "Reset the 6502 and start from $FFFC", cmd_reset },
  { "irq",  "Raise a pending IRQ", cmd_irq },
  { "nmi",  "Raise a pending NMI", cmd_nmi },
  { "q",    "Quit 6502EMU", cmd_q },
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  if (arg == NULL) {
    for (int i = 0; i < NR_CMD; i++) {
      printf("%-7s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
    return 0;
  }
  for (int i = 0; i < NR_CMD; i++) {
    if (strcmp(arg, cmd_table[i].name) == 0) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
      return 0;
    }
  }
  printf("Unknown command '%s'\n", arg);
  return 0;
}

static int cmd_c(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  uint64_t limit = monitor_max_inst();
  if (arg != NULL) limit = parse_count(arg, 0);
  if (limit == 0) limit = (uint64_t)-1;
  cpu_exec(limit);
  if (nemu_state.state == NEMU_STOP) {
    printf("Execution stopped.\n");
    isa_reg_display();
  }
  return 0;
}

static int cmd_si(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  uint64_t n = parse_count(arg, 1);
  if (n == 0) n = 1;
  cpu_exec(n);
  if (nemu_state.state == NEMU_STOP || nemu_state.state == NEMU_RUNNING) {
    isa_reg_display();
  }
  return 0;
}

static int cmd_info(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  if (arg == NULL || strcmp(arg, "r") == 0 || strcmp(arg, "reg") == 0) {
    isa_reg_display();
    return 0;
  }
  printf("Usage: info r\n");
  return 0;
}

static int cmd_x(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  uint32_t addr;
  uint64_t len = 64;
  if (arg == NULL) {
    addr = (uint32_t)cpu.pc & 0xFFFF;
  } else if (!parse_addr(arg, &addr)) {
    printf("Usage: x ADDR [LEN]\n");
    return 0;
  }
  char *len_arg = next_arg(&save);
  if (len_arg != NULL) len = parse_count(len_arg, 64);
  if (len == 0) len = 1;
  if (len > MAX_XDUMP) len = MAX_XDUMP;
  if (addr > 0xFFFF) { printf("address out of range\n"); return 0; }
  if ((uint64_t)addr + len > 0x10000) len = 0x10000 - addr;

  for (uint64_t i = 0; i < len; i += 16) {
    printf("%04X:", (unsigned)(addr + i));
    uint64_t j;
    for (j = 0; j < 16 && i + j < len; j++) {
      printf(" %02X", (unsigned)(vaddr_read(addr + i + j, 1) & 0xFF));
    }
    for (; j < 16; j++) printf("   ");
    printf("  ");
    for (j = 0; j < 16 && i + j < len; j++) {
      uint8_t c = (uint8_t)vaddr_read(addr + i + j, 1);
      putchar((c >= 32 && c < 127) ? c : '.');
    }
    putchar('\n');
  }
  return 0;
}

#ifdef CONFIG_ISA_m6502
static int cmd_d(char *args) {
  char *save = args;
  char *arg = next_arg(&save);
  uint32_t addr = (uint32_t)cpu.pc & 0xFFFF;
  uint64_t count = 10;
  if (arg != NULL && !parse_addr(arg, &addr)) {
    printf("Usage: d [ADDR [COUNT]]\n");
    return 0;
  }
  char *count_arg = next_arg(&save);
  if (count_arg != NULL) count = parse_count(count_arg, 10);
  if (count == 0) count = 1;
  if (count > 256) count = 256;
  if (addr > 0xFFFF) { printf("address out of range\n"); return 0; }

  for (uint64_t i = 0; i < count; i++) {
    uint8_t code[3];
    for (int j = 0; j < 3; j++) code[j] = (uint8_t)(vaddr_read((uint16_t)(addr + j), 1) & 0xFF);
    int len = m6502_inst_length(code[0]);
    char text[128];
    disassemble(text, sizeof(text), addr, code, len);
    printf("%04X: ", (unsigned)addr);
    for (int j = 0; j < len; j++) printf("%02X ", code[j]);
    for (int j = len; j < 3; j++) printf("   ");
    printf(" %s\n", text);
    addr = (uint16_t)(addr + len);
  }
  return 0;
}
#else
static int cmd_d(char *args) {
  (void)args;
  printf("disassembly is only available in the m6502 configuration\n");
  return 0;
}
#endif

static int cmd_set(char *args) {
  char *save = args;
  char *reg = next_arg(&save);
  char *val = next_arg(&save);
  uint32_t v;
  if (reg == NULL || val == NULL || !parse_addr(val, &v)) {
    printf("Usage: set REG VALUE   (reg: a x y sp p pc cycles)\n");
    return 0;
  }
#ifdef CONFIG_ISA_m6502
  if (strcasecmp(reg, "a") == 0) cpu.a = (uint8_t)v;
  else if (strcasecmp(reg, "x") == 0) cpu.x = (uint8_t)v;
  else if (strcasecmp(reg, "y") == 0) cpu.y = (uint8_t)v;
  else if (strcasecmp(reg, "sp") == 0) cpu.sp = (uint8_t)v;
  else if (strcasecmp(reg, "p") == 0) cpu.p = (uint8_t)v;
  else if (strcasecmp(reg, "pc") == 0) cpu.pc = v & 0xFFFF;
  else if (strcasecmp(reg, "cycles") == 0 || strcasecmp(reg, "cyc") == 0) cpu.cycles = v;
  else { printf("unknown register '%s'\n", reg); return 0; }
  isa_reg_display();
#else
  printf("register editing is only available in the m6502 configuration\n");
#endif
  return 0;
}

static int cmd_load(char *args) {
  char *save = args;
  char *file = next_arg(&save);
  if (file == NULL) { printf("Usage: load FILE [ADDR]\n"); return 0; }
  uint32_t addr = (uint32_t)cpu.pc & 0xFFFF;
  char *addr_arg = next_arg(&save);
  if (addr_arg != NULL && !parse_addr(addr_arg, &addr)) {
    printf("invalid load address\n");
    return 0;
  }
  if (addr > 0xFFFF) { printf("address out of range\n"); return 0; }

  FILE *fp = fopen(file, "rb");
  if (fp == NULL) { printf("cannot open '%s'\n", file); return 0; }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (size < 0 || (uint64_t)addr + (uint64_t)size > 0x10000) {
    printf("file does not fit in 64 KiB memory\n");
    fclose(fp);
    return 0;
  }
  if (size > 0 && fread(guest_to_host(addr), (size_t)size, 1, fp) != 1) {
    printf("read error\n");
    fclose(fp);
    return 0;
  }
  fclose(fp);
  printf("loaded %ld bytes at $%04X\n", size, (unsigned)addr);
  return 0;
}

static int cmd_reset(char *args) {
  (void)args;
#ifdef CONFIG_ISA_m6502
  m6502_reset();
#endif
  nemu_state.state = NEMU_STOP;
  nemu_state.halt_ret = 0;
  isa_reg_display();
  return 0;
}

static int cmd_irq(char *args) {
  (void)args;
#ifdef CONFIG_ISA_m6502
  m6502_request_irq();
  printf("IRQ pending\n");
#else
  printf("IRQ is only available in the m6502 configuration\n");
#endif
  return 0;
}

static int cmd_nmi(char *args) {
  (void)args;
#ifdef CONFIG_ISA_m6502
  m6502_request_nmi();
  printf("NMI pending\n");
#else
  printf("NMI is only available in the m6502 configuration\n");
#endif
  return 0;
}

static int cmd_q(char *args) {
  (void)args;
  return -1;
}

void sdb_set_batch_mode(void) { is_batch_mode = true; }

void sdb_mainloop(void) {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);
    char *cmd = strtok(str, " \t");
    if (cmd == NULL) continue;

    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) args = NULL;

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) return;
        break;
      }
    }
    if (i == NR_CMD) printf("Unknown command '%s'\n", cmd);
  }
}

void init_sdb(void) {
  init_regex();
  init_wp_pool();
}
