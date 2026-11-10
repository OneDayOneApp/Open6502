#include "Vopen6502_cpu.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static Vopen6502_cpu *dut;
static std::vector<unsigned char> mem(65536, 0xEA);

static void tick() {
  dut->clk = 0;
  dut->eval();
  dut->cpu_en = 1;
  dut->bus_rdata = mem[dut->bus_addr];
  dut->eval();
  dut->clk = 1;
  dut->eval();
  if (dut->bus_we) mem[dut->bus_addr] = dut->bus_wdata;
  dut->clk = 0;
  dut->cpu_en = 0;
  dut->eval();
}

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  if (argc < 2) { std::fprintf(stderr, "usage: %s klaus.bin\n", argv[0]); return 2; }
  FILE *f = std::fopen(argv[1], "rb");
  if (!f) { std::perror(argv[1]); return 2; }
  std::fread(mem.data(), 1, mem.size(), f);
  std::fclose(f);
  mem[0xFFFC] = 0x00;
  mem[0xFFFD] = 0x04;

  dut = new Vopen6502_cpu;
  dut->rst_n = 0;
  dut->cpu_en = 0;
  dut->irq_n = 1;
  dut->nmi_n = 1;
  for (int i = 0; i < 8; i++) tick();
  dut->rst_n = 1;
  for (long long c = 0; c < 200000000LL; c++) {
    tick();
    if (dut->dbg_pc == 0x3469) {
      std::printf("PASS: Open6502 RTL Klaus PC=%04x cycles=%lld\n", dut->dbg_pc, c);
      return 0;
    }
  }
  std::printf("FAIL: Open6502 RTL Klaus PC=%04x\n", dut->dbg_pc);
  return 1;
}
