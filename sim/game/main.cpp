#include "Vopen6502_top.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static Vopen6502_top *dut;
static const unsigned int nespal[64] = {
  0x666666,0x002A88,0x1412A7,0x3B00A4,0x5C007E,0x6E0040,0x6C0600,0x561D00,
  0x333500,0x0B4800,0x005200,0x004F08,0x00404D,0x000000,0x000000,0x000000,
  0xADADAD,0x155FD9,0x4240FF,0x7527FE,0xA01ACC,0xB71E7B,0xB53120,0x994E00,
  0x6B6D00,0x388700,0x0C9300,0x008F32,0x007C8D,0x000000,0x000000,0x000000,
  0xFFFEFF,0x64B0FF,0x9290FF,0xC676FF,0xF36AFF,0xFE6ECC,0xFE8170,0xEA9E22,
  0xBCBE00,0x88D800,0x5CE430,0x45E082,0x48CDDE,0x4F4F4F,0x000000,0x000000,
  0xFFFEFF,0xC4E0FF,0xD3D2FF,0xE8C8FF,0xFBC2FF,0xFEC4EA,0xFECCC5,0xF7D8A5,
  0xE4E594,0xCFEF96,0xBDF4AB,0xB3F3CC,0xB5EBF2,0xB8B8B8,0x000000,0x000000,
};
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
  for (int i = 0; i < 4; i++) tick();
  for (size_t i = 0; i < chr.size(); i++) {
    dut->chr_load_addr = i & 0x1fff; dut->chr_load_data = chr[i]; dut->chr_load_we = 1;
    tick();
  }
  dut->chr_load_we = 0;

  std::vector<unsigned char> cur(256 * 240, 0xFF), shot;
  int frames = 0, last_vsync = 1, target = 5;
  bool have = false;
  for (long c = 0; c < 30000000; c++) {
    tick();
    if (dut->vid_de && dut->dbg_ppu_x < 256 && dut->dbg_ppu_y < 240) {
      cur[dut->dbg_ppu_y * 256 + dut->dbg_ppu_x] = dut->dbg_ppu_pixel & 0x3F;
    }
    if (dut->vid_vsync != last_vsync) {
      last_vsync = dut->vid_vsync;
      if (!last_vsync) {
        frames++;
        if (frames == target) { shot = cur; have = true; break; }
        std::fill(cur.begin(), cur.end(), 0xFF);
      }
    }
  }
  int nonblack = 0;
  for (auto p : shot) if (p != 0xFF && (p & 0x3F) != 0x0F) nonblack++;
  if (have) {
    FILE *f = std::fopen(argv[3], "wb");
    std::fprintf(f, "P6\n256 240\n255\n");
    for (int y = 0; y < 240; y++) for (int x = 0; x < 256; x++) {
      unsigned int c = nespal[shot[y*256+x] & 0x3F];
      unsigned char rgb[3] = {(unsigned char)(c >> 16), (unsigned char)(c >> 8), (unsigned char)c};
      std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::string ipath = std::string(argv[3]) + ".idx";
    FILE *fi = std::fopen(ipath.c_str(), "wb");
    std::fwrite(shot.data(), 1, shot.size(), fi);
    std::fclose(fi);
  }
  std::printf("frames=%d nonblack=%d pc=%04x ctrl=%02x mask=%02x status=%02x\n",
      frames, nonblack, dut->dbg_cpu_pc, dut->dbg_ppu_ctrl,
      dut->dbg_ppu_mask, dut->dbg_ppu_status);
  std::printf("pixel=%02x pal0=%02x pal1=%02x pal2=%02x chr0=%02x\n", dut->dbg_ppu_pixel, dut->dbg_pal0, dut->dbg_pal1, dut->dbg_pal2, dut->dbg_chr0);
  std::printf("dma_busy=%d dma_state=%d dma_count=%02x\n", dut->dbg_dma_busy, dut->dbg_dma_state, dut->dbg_dma_count);
  return have && nonblack ? 0 : 1;
}
