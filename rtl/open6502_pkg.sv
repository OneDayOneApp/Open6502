package open6502_pkg;
  localparam int unsigned MASTER_HZ   = 21_477_272;
  localparam int unsigned CPU_DIV     = 12;
  localparam int unsigned PPU_DIV     = 4;
  localparam int unsigned SCREEN_W    = 256;
  localparam int unsigned SCREEN_H    = 240;
  localparam int unsigned PPU_DOTS    = 341;
  localparam int unsigned PPU_LINES   = 262;

  typedef enum logic [1:0] {
    DBG_CPU = 2'd0,
    DBG_BUS = 2'd1,
    DBG_PPU = 2'd2,
    DBG_APU = 2'd3
  } dbg_kind_t;
endpackage
