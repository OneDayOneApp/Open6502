#include "Vopen6502_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static Vopen6502_top *dut;
static std::vector<unsigned char> prg, chr;

static void tick() {
  dut->cart_rdata = prg[dut->cart_addr & (prg.size() - 1)];
  dut->clk_21m = 0; dut->eval();
  dut->cart_rdata = prg[dut->cart_addr & (prg.size() - 1)];
  dut->clk_21m = 1; dut->eval();
}

double sc_time_stamp() { return 0.0; }

int main(int argc, char **argv) {
  Verilated::commandArgs(argc, argv);
  if (argc < 4) { std::fprintf(stderr, "usage: %s prg.bin chr.bin out.ppm\n", argv[0]); return 2; }
  for (int i = 0; i < 2; i++) {
    FILE *f = std::fopen(argv[1+i], "rb");
    if (!f) { std::perror(argv[1+i]); return 2; }
    std::fseek(f, 0, SEEK_END); long n = std::ftell(f); std::fseek(f, 0, SEEK_SET);
    auto &v = i ? chr : prg; v.resize(n); std::fread(v.data(), 1, n, f); std::fclose(f);
  }
  dut = new Vopen6502_top;
  dut->rst_n = 0; dut->chr_load_we = 0; dut->joy1 = 0; dut->joy2 = 0;
  for (int i = 0; i < 8; i++) tick();
  dut->rst_n = 1;
  for (size_t i = 0; i < chr.size(); i++) {
    dut->chr_load_addr = i & 0x1fff; dut->chr_load_data = chr[i]; dut->chr_load_we = 1;
    tick();
  }
  dut->chr_load_we = 0;

  std::vector<unsigned char> fb(256 * 240);
  int frames = 0, nonblack = 0, de_count = 0, last_vsync = 1;
  bool got = false;
  for (long c = 0; c < 20000000; c++) {
    tick();
    if (dut->vid_de) {
      if (de_count < 256 * 240) fb[de_count] = dut->vid_rgb;
      de_count++;
      if (dut->vid_rgb) nonblack++;
    }
    if (dut->vid_vsync != last_vsync) {
      last_vsync = dut->vid_vsync;
      if (!last_vsync) {
        frames++;
        if (!got) {
          got = true;
          FILE *f = std::fopen(argv[3], "wb");
          std::fprintf(f, "P6\n256 240\n255\n");
          for (int y = 0; y < 240; y++) for (int x = 0; x < 256; x++) {
            unsigned char p = fb[y*256+x]; unsigned char rgb[3] = {(unsigned char)((p&0xf)*17),(unsigned char)(((p>>4)&0xf)*17),0};
            std::fwrite(rgb, 1, 3, f);
          }
          std::fclose(f);
        }
      }
    }
  }
  std::printf("frames=%d de=%d nonblack=%d pc=%04x ctrl=%02x mask=%02x status=%02x\n",
      frames, de_count, nonblack, dut->dbg_cpu_pc, dut->dbg_ppu_ctrl,
      dut->dbg_ppu_mask, dut->dbg_ppu_status);
  std::printf("pixel=%02x pal0=%02x pal1=%02x pal2=%02x chr0=%02x\n", dut->dbg_ppu_pixel, dut->dbg_pal0, dut->dbg_pal1, dut->dbg_pal2, dut->dbg_chr0);
  std::printf("dma_busy=%d dma_state=%d dma_count=%02x\n", dut->dbg_dma_busy, dut->dbg_dma_state, dut->dbg_dma_count);
  return nonblack ? 0 : 1;
}
