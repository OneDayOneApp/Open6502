`timescale 1ns/1ps
module open6502_controller_tb;
  logic clk = 0, rst_n = 0;
  logic [12:0] chr_load_addr = 0;
  logic [7:0] chr_load_data = 0, chr_load_we = 0;
  logic [7:0] joy1 = 8'h08, joy2 = 0;
  logic [15:0] cart_addr;
  logic [7:0] cart_wdata, cart_rdata;
  logic cart_we, cart_irq;
  logic vid_pclk, vid_de, vid_hsync, vid_vsync;
  logic [11:0] vid_rgb;
  logic aud_valid;
  logic signed [15:0] aud_sample;
  logic dbg_valid;
  logic [63:0] dbg_payload;
  logic [15:0] dbg_cpu_pc;
  logic [7:0] dbg_ctrl, dbg_mask, dbg_status, dbg_pal0, dbg_pal1, dbg_pal2;
  logic [8:0] dbg_x, dbg_y;
  logic [15:0] dbg_nmi, dbg_pw;
  logic [7:0] dbg_plast, dbg_zp0a;
  logic [4:0] dbg_pa;
  logic dbg_dma;

  logic [7:0] prg [0:32767];
  assign cart_rdata = prg[cart_addr[14:0]];
  always #23.3 clk = ~clk;

  open6502_top #(.MAPPER(3'd0)) dut (
    .clk_21m(clk), .rst_n(rst_n),
    .chr_load_addr(chr_load_addr), .chr_load_data(chr_load_data),
    .chr_load_we(chr_load_we), .joy1(joy1), .joy2(joy2),
    .vid_pclk(vid_pclk), .vid_de(vid_de), .vid_hsync(vid_hsync),
    .vid_vsync(vid_vsync), .vid_rgb(vid_rgb),
    .aud_valid(aud_valid), .aud_sample(aud_sample),
    .cart_addr(cart_addr), .cart_wdata(cart_wdata), .cart_rdata(cart_rdata),
    .cart_we(cart_we), .cart_irq(cart_irq),
    .dbg_valid(dbg_valid), .dbg_payload(dbg_payload),
    .dbg_cpu_pc(dbg_cpu_pc), .dbg_ppu_ctrl(dbg_ctrl),
    .dbg_ppu_mask(dbg_mask), .dbg_ppu_status(dbg_status),
    .dbg_ppu_pixel(), .dbg_pal0(dbg_pal0), .dbg_pal1(dbg_pal1),
    .dbg_pal2(dbg_pal2), .dbg_chr0(), .dbg_ppu_x(dbg_x), .dbg_ppu_y(dbg_y),
    .dbg_nmi_hits(dbg_nmi), .dbg_pal_writes(dbg_pw), .dbg_pal_last(dbg_plast),
    .dbg_pal_addr(dbg_pa), .dbg_zp0e(), .dbg_zp0f(), .dbg_zp10(),
    .dbg_zp11(), .dbg_dma_busy(dbg_dma), .dbg_dma_state(), .dbg_dma_count(),
    .dbg_zp0a(dbg_zp0a)
  );

  initial begin
    for (int i = 0; i < 32768; i++) prg[i] = 8'hEA;
    for (int i = 0; i < 2048; i++) dut.u_ram.mem[i] = 8'h00;
    // $8000: write $4016=1, write $4016=0, read 8 bits into $0A
    prg[0] = 8'hA9; prg[1] = 8'h01; prg[2] = 8'h8D; prg[3] = 8'h16; prg[4] = 8'h40;
    prg[5] = 8'hA9; prg[6] = 8'h00; prg[7] = 8'h8D; prg[8] = 8'h16; prg[9] = 8'h40;
    prg[10] = 8'hA2; prg[11] = 8'h08;
    prg[12] = 8'hAD; prg[13] = 8'h16; prg[14] = 8'h40;
    prg[15] = 8'h4A; prg[16] = 8'h2E; prg[17] = 8'h0A; prg[18] = 8'h00;
    prg[19] = 8'hCA; prg[20] = 8'hD0; prg[21] = 8'hF6;
    prg[22] = 8'h4C; prg[23] = 8'h16; prg[24] = 8'h80;
    prg[32764] = 8'h00; prg[32765] = 8'h80;
    repeat (8) @(posedge clk); rst_n = 1;
    repeat (2000) @(posedge clk);
    $display("controller $0A=%02x pc=%04x", dut.u_ram.mem[11'h00A], dbg_cpu_pc);
    if (dut.u_ram.mem[11'h00A] !== 8'h10)
      $fatal(1, "controller START bit wrong (expected bit4): %02x", dut.u_ram.mem[11'h00A]);
    $display("PASS: Open6502 controller START bit (bit4, reversed bit order)");
    $finish;
  end
endmodule
