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

#include <errno.h>
#include <isa.h>
#include <memory/paddr.h>

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm();

static void welcome(void) {
  Log("Trace: %s", MUXDEF(CONFIG_TRACE, ANSI_FMT("ON", ANSI_FG_GREEN), ANSI_FMT("OFF", ANSI_FG_RED)));
  IFDEF(CONFIG_TRACE, Log("If trace is enabled, a log file will be generated "
        "to record the trace. This may lead to a large log file. "
        "If it is not necessary, you can disable it in menuconfig"));
  Log("Build time: %s, %s", __TIME__, __DATE__);
#ifdef CONFIG_ISA_m6502
  printf("Welcome to %s!\n", ANSI_FMT("6502EMU", ANSI_FG_YELLOW ANSI_BG_RED));
  printf("A NEMU-based MOS 6502 simulator.  For help, type \"help\".\n");
#else
  printf("Welcome to %s-NEMU!\n", ANSI_FMT(str(__GUEST_ISA__), ANSI_FG_YELLOW ANSI_BG_RED));
  printf("For help, type \"help\"\n");
#endif
}

/* ISAs that do not need a post-load hook (all except m6502) use this. */
__attribute__((weak)) void isa_post_load(long img_size, paddr_t load_addr,
    vaddr_t entry, bool use_reset) {
  (void)img_size; (void)load_addr; (void)entry; (void)use_reset;
}

#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static int difftest_port = 1234;
static paddr_t load_addr = (paddr_t)-1;
static vaddr_t entry_pc = (vaddr_t)-1;
static bool use_reset_vector = false;
static bool use_65c02 = false;
static vaddr_t stop_pc = (vaddr_t)-1;
uint64_t g_max_inst = 0;  /* consumed by the simple debugger */

uint64_t monitor_max_inst(void) { return g_max_inst; }


static long load_img(void) {
  if (img_file == NULL) {
    Log("No image is given. Use the built-in image.");
    if (load_addr == (paddr_t)-1) load_addr = RESET_VECTOR;
    return 0;
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  Assert(size >= 0, "Can not determine the size of '%s'", img_file);

  if (load_addr == (paddr_t)-1) load_addr = RESET_VECTOR;
  Assert(load_addr >= PMEM_LEFT && load_addr <= PMEM_RIGHT,
      "load address 0x%x is outside memory [0x%x, 0x%x]",
      (unsigned)load_addr, (unsigned)PMEM_LEFT, (unsigned)PMEM_RIGHT);
  Assert((uint64_t)load_addr + (uint64_t)size <= (uint64_t)PMEM_RIGHT + 1,
      "image of %ld bytes at 0x%x does not fit in memory", size, (unsigned)load_addr);

  Log("The image is %s, size = %ld, load address = 0x%x", img_file, size, (unsigned)load_addr);

  fseek(fp, 0, SEEK_SET);
  if (size > 0) {
    size_t ret = fread(guest_to_host(load_addr), (size_t)size, 1, fp);
    assert(ret == 1);
  }

  fclose(fp);
  return size;
}

static void usage(const char *prog) {
  printf("Usage: %s [OPTION...] IMAGE\n\n", prog);
  printf("\t-b,--batch                 run in batch mode\n");
  printf("\t-l,--log=FILE              write NEMU log/trace to FILE\n");
  printf("\t-a,--load-addr=ADDR        load raw image at ADDR (default 0x8000)\n");
  printf("\t-e,--pc=ADDR               start execution at ADDR\n");
  printf("\t-r,--reset                 start from the 6502 reset vector at $FFFC\n");
  printf("\t-n,--max-inst=N            stop after N guest instructions\n");
  printf("\t-s,--stop-pc=ADDR          stop when the guest PC reaches ADDR\n");
  printf("\t-C,--65c02                 enable 65C02 extensions\n");
  printf("\t-d,--diff=REF_SO           run DiffTest with reference REF_SO\n");
  printf("\t-p,--port=PORT             run DiffTest with port PORT\n");
  printf("\t-h,--help                  show this help\n\n");
  printf("IMAGE is a raw 6502 binary.  Example:\n");
  printf("  %s --load-addr=0x0000 --pc=0x0400 6502_functional_test.bin\n", prog);
}

static unsigned long parse_num(const char *s, const char *what) {
  char *end = NULL;
  errno = 0;
  unsigned long val = strtoul(s, &end, 0);
  Assert(*s != '\0' && end != NULL && *end == '\0' && errno == 0,
      "invalid %s: '%s'", what, s);
  return val;
}

static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"batch"    , no_argument      , NULL, 'b'},
    {"log"      , required_argument, NULL, 'l'},
    {"diff"     , required_argument, NULL, 'd'},
    {"port"     , required_argument, NULL, 'p'},
    {"help"     , no_argument      , NULL, 'h'},
    {"load-addr", required_argument, NULL, 'a'},
    {"pc"       , required_argument, NULL, 'e'},
    {"reset"    , no_argument      , NULL, 'r'},
    {"max-inst" , required_argument, NULL, 'n'},
    {"stop-pc"  , required_argument, NULL, 's'},
    {"65c02"    , no_argument      , NULL, 'C'},
    {"cpu"      , required_argument, NULL, 'C'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ((o = getopt_long(argc, argv, "-bhl:d:p:a:e:rn:s:C", table, NULL)) != -1) {
    switch (o) {
      case 'b': sdb_set_batch_mode(); break;
      case 'p': difftest_port = (int)parse_num(optarg, "port"); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 'a': load_addr = (paddr_t)parse_num(optarg, "load address"); break;
      case 'e': entry_pc = (vaddr_t)parse_num(optarg, "entry PC"); break;
      case 'r': use_reset_vector = true; break;
      case 'n': g_max_inst = (uint64_t)parse_num(optarg, "instruction limit"); break;
      case 's': stop_pc = (vaddr_t)parse_num(optarg, "stop PC"); break;
      case 'C':
        if (optarg == NULL || strcmp(optarg, "65c02") == 0 || strcmp(optarg, "65C02") == 0) {
          use_65c02 = true;
        } else if (strcmp(optarg, "6502") == 0 || strcmp(optarg, "2a03") == 0) {
          use_65c02 = false;
        } else {
          panic("unknown CPU variant '%s'", optarg);
        }
        break;
      case 'h': usage(argv[0]); exit(0);
      case 1: img_file = optarg; break;
      default: usage(argv[0]); exit(0);
    }
  }
  return 0;
}

void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */
  parse_args(argc, argv);
  init_rand();
  init_log(log_file);
  init_mem();
  IFDEF(CONFIG_DEVICE, init_device());

  /* Built-in image, then optionally replace it with the user image. */
  init_isa();
  long img_size = load_img();
  isa_post_load(img_size, load_addr, entry_pc, use_reset_vector);
#ifdef CONFIG_ISA_m6502
  cpu.is_65c02 = use_65c02;
#endif
  g_stop_pc = stop_pc;

  init_difftest(diff_so_file, img_size, difftest_port);
  init_sdb();
  IFDEF(CONFIG_ITRACE, init_disasm());
  welcome();
}
#else // CONFIG_TARGET_AM
static long load_img(void) {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

void am_init_monitor(void) {
  init_rand();
  init_mem();
  init_isa();
  long size = load_img();
  isa_post_load(size, RESET_VECTOR, (vaddr_t)-1, false);
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
