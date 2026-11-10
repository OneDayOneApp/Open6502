module open6502_top (
  input  logic        clk_21m,
  input  logic        rst_n,

  output logic        vid_pclk,
  output logic        vid_de,
  output logic        vid_hsync,
  output logic        vid_vsync,
  output logic [11:0] vid_rgb,

  output logic        aud_valid,
  output logic signed [15:0] aud_sample,

  input  logic [7:0]  joy1,
  input  logic [7:0]  joy2,

  output logic [15:0] cart_addr,
  output logic [7:0]  cart_wdata,
  input  logic [7:0]  cart_rdata,
  output logic        cart_we,
  input  logic        cart_irq,

  output logic        dbg_valid,
  output logic [63:0] dbg_payload
);
  import open6502_pkg::*;

  logic        rst_sync_n;
  logic        cpu_en;
  logic        ppu_en;
  logic        aud_en;
  logic [8:0]  ppu_x;
  logic [8:0]  ppu_y;
  logic        frame_tick;
  logic        joy1_serial;
  logic        joy2_serial;
  logic        dbg_valid_i;
  logic [63:0] dbg_payload_i;

  open6502_clocks u_clocks (
    .clk_21m(clk_21m), .rst_n(rst_n),
    .rst_sync_n(rst_sync_n), .cpu_en(cpu_en),
    .ppu_en(ppu_en), .aud_en(aud_en)
  );

  open6502_video_timing u_video (
    .clk(clk_21m), .rst_n(rst_sync_n), .ppu_en(ppu_en),
    .x(ppu_x), .y(ppu_y),
    .de(vid_de), .hsync(vid_hsync), .vsync(vid_vsync),
    .frame_tick(frame_tick)
  );

  open6502_controller u_joy1 (
    .clk(clk_21m), .rst_n(rst_sync_n),
    .strobe(1'b0), .shift_en(1'b0),
    .buttons(joy1), .serial(joy1_serial)
  );

  open6502_controller u_joy2 (
    .clk(clk_21m), .rst_n(rst_sync_n),
    .strobe(1'b0), .shift_en(1'b0),
    .buttons(joy2), .serial(joy2_serial)
  );

  open6502_difftest_bridge u_dbg (
    .clk(clk_21m), .rst_n(rst_sync_n),
    .valid(1'b0), .kind(2'd0), .tick(64'd0),
    .addr(16'd0), .data(8'd0), .we(1'b0), .source(8'd0),
    .dbg_valid(dbg_valid_i), .dbg_payload(dbg_payload_i)
  );

  assign vid_pclk = clk_21m;
  assign vid_rgb = 12'h000;
  assign aud_valid = aud_en;
  assign aud_sample = 16'sd0;
  assign cart_addr = 16'd0;
  assign cart_wdata = 8'd0;
  assign cart_we = 1'b0;
  assign dbg_valid = dbg_valid_i;
  assign dbg_payload = dbg_payload_i;

  // Keep controller/cart/joy signals observable while the CPU/PPU/APU
  // implementations are brought up in later phases.
  logic unused;
  assign unused = ^{joy1_serial, joy2_serial, cart_rdata, cart_irq,
                    frame_tick, ppu_x, ppu_y, cpu_en};
endmodule
