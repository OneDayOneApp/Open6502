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
  logic [15:0] cpu_addr;
  logic [7:0]  cpu_wdata;
  logic [7:0]  cpu_rdata;
  logic        cpu_we;
  logic [7:0]  ram_rdata;
  logic [10:0] ram_addr;
  logic        ram_sel, ppu_sel, apu_sel, cart_sel;
  logic        ram_we, ppu_we, apu_we, cart_we_i;
  logic [7:0]  ppu_rdata;
  logic        ppu_nmi;
  logic [7:0]  ppu_pixel;
  logic        ppu_sprite0;
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

  open6502_cpu u_cpu (
    .clk(clk_21m), .rst_n(rst_sync_n), .cpu_en(cpu_en),
    .bus_rdata(cpu_rdata), .irq_n(1'b1), .nmi_n(~ppu_nmi),
    .bus_addr(cpu_addr), .bus_wdata(cpu_wdata), .bus_we(cpu_we),
    .dbg_pc(), .dbg_a(), .dbg_x(), .dbg_y(), .dbg_sp(), .dbg_p()
  );

  open6502_bus u_bus (
    .cpu_addr(cpu_addr), .cpu_we(cpu_we), .cpu_wdata(cpu_wdata),
    .ram_rdata(ram_rdata), .ppu_rdata(ppu_rdata), .apu_rdata(8'h00),
    .cart_rdata(cart_rdata),
    .ram_sel(ram_sel), .ppu_sel(ppu_sel), .apu_sel(apu_sel),
    .cart_sel(cart_sel), .ram_we(ram_we), .ppu_we(ppu_we),
    .apu_we(apu_we), .cart_we(cart_we_i), .ram_addr(ram_addr),
    .cpu_rdata(cpu_rdata)
  );

  open6502_wram u_ram (
    .clk(clk_21m), .rst_n(rst_sync_n), .we(ram_we),
    .addr(ram_addr), .wdata(cpu_wdata), .rdata(ram_rdata)
  );

  open6502_ppu u_ppu (
    .clk(clk_21m), .rst_n(rst_sync_n), .ppu_en(ppu_en),
    .x(ppu_x), .y(ppu_y),
    .cs(ppu_sel), .we(ppu_we), .addr(cpu_addr[2:0]),
    .wdata(cpu_wdata), .rdata(ppu_rdata), .pixel_index(ppu_pixel),
    .sprite0_hit(ppu_sprite0), .nmi(ppu_nmi)
  );

  open6502_difftest_bridge u_dbg (
    .clk(clk_21m), .rst_n(rst_sync_n),
    .valid(1'b0), .kind(2'd0), .tick(64'd0),
    .addr(16'd0), .data(8'd0), .we(1'b0), .source(8'd0),
    .dbg_valid(dbg_valid_i), .dbg_payload(dbg_payload_i)
  );

  assign vid_pclk = clk_21m;
  assign vid_rgb = {ppu_pixel[3:0], ppu_pixel[3:0], ppu_pixel[3:0]};
  assign aud_valid = aud_en;
  assign aud_sample = 16'sd0;
  assign cart_addr = cpu_addr;
  assign cart_wdata = cpu_wdata;
  assign cart_we = cart_we_i;
  assign dbg_valid = dbg_valid_i;
  assign dbg_payload = dbg_payload_i;

  // Keep controller/cart/joy signals observable while the CPU/PPU/APU
  // implementations are brought up in later phases.
  logic unused;
  assign unused = ^{joy1_serial, joy2_serial, cart_irq, frame_tick,
                    ppu_x, ppu_y, ram_sel, ppu_sel, apu_sel, cart_sel,
                    ppu_we, apu_we};
endmodule
