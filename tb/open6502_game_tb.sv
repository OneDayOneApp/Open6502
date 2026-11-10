`timescale 1ns/1ps
module open6502_game_tb;
  localparam int PRG_WORDS = 32768;
  localparam int CHR_WORDS = 8192;

  logic clk = 0, rst_n = 0;
  logic [12:0] chr_load_addr = 0;
  logic [7:0]  chr_load_data = 0;
  logic        chr_load_we = 0;
  logic [7:0] joy1 = 0, joy2 = 0;
  logic [15:0] cart_addr;
  logic [7:0] cart_wdata, cart_rdata;
  logic cart_we, cart_irq;
  logic vid_pclk, vid_de, vid_hsync, vid_vsync;
  logic [11:0] vid_rgb;
  logic aud_valid;
  logic signed [15:0] aud_sample;
  logic dbg_valid;
  logic [63:0] dbg_payload;

  logic [7:0] prg [0:PRG_WORDS-1];
  logic [7:0] chr [0:CHR_WORDS-1];

  open6502_top #(.MAPPER(3'd0)) dut (
    .clk_21m(clk), .rst_n(rst_n),
    .chr_load_addr(chr_load_addr), .chr_load_data(chr_load_data),
    .chr_load_we(chr_load_we),
    .joy1(joy1), .joy2(joy2),
    .vid_pclk(vid_pclk), .vid_de(vid_de), .vid_hsync(vid_hsync),
    .vid_vsync(vid_vsync), .vid_rgb(vid_rgb),
    .aud_valid(aud_valid), .aud_sample(aud_sample),
    .cart_addr(cart_addr), .cart_wdata(cart_wdata),
    .cart_rdata(cart_rdata), .cart_we(cart_we), .cart_irq(cart_irq),
    .dbg_valid(dbg_valid), .dbg_payload(dbg_payload)
  );

  assign cart_rdata = prg[cart_addr[14:0]];
  always #23.3 clk = ~clk;

  int pixels = 0, nonblack = 0, frames = 0, vblank_cycles = 0;
  int reads2002 = 0, vreads2002 = 0;
  logic vsync_d = 1;

  always @(posedge clk) begin
    if (dut.u_cpu.bus_addr == 16'h2002 && !dut.u_cpu.bus_we) begin
      reads2002++;
      if (dut.u_cpu.bus_rdata[7]) vreads2002++;
    end
    if (dut.u_ppu.status[7]) vblank_cycles++;
    if (vid_de) begin
      pixels++;
      if (vid_rgb != 0) nonblack++;
    end
    if (vid_vsync !== vsync_d) begin
      vsync_d <= vid_vsync;
      if (!vid_vsync) frames++;
    end
  end

  initial begin
    $readmemh("/tmp/falling/falling_prg.hex", prg);
    $readmemh("/tmp/falling/falling_chr.hex", chr);
    repeat (8) @(posedge clk);
    rst_n = 1;
    // Preload the 8 KiB CHR RAM from the cartridge CHR ROM.
    for (int i = 0; i < CHR_WORDS; i++) begin
      @(posedge clk);
      chr_load_addr = i[12:0];
      chr_load_data = chr[i];
      chr_load_we = 1;
    end
    @(posedge clk); chr_load_we = 0;
    repeat (2_000_000) @(posedge clk);
    $display("RTL game: frames=%0d de_pixels=%0d nonblack=%0d", frames, pixels, nonblack);
    $display("READ2002=%0d VREAD2002=%0d", reads2002, vreads2002);
    $display("VBLANK cycles=%0d cpu_en=%0d dma_busy=%0d dmc_stall=%0d cpu_state=%0d A=%02x P=%02x bus=%02x",
      vblank_cycles, dut.cpu_en, dut.dma_busy, dut.dmc_stall, dut.u_cpu.state,
      dut.u_cpu.a, dut.u_cpu.p, dut.u_cpu.bus_rdata);
    $display("DBG PC=%04x ctrl=%02x mask=%02x status=%02x pal0=%02x pixel=%02x chr0=%02x ppu_en=%0d x=%0d y=%0d",
      dut.u_cpu.pc, dut.u_ppu.ctrl, dut.u_ppu.mask, dut.u_ppu.status, dut.u_ppu.pal[0],
      dut.u_ppu.pixel_index, dut.u_ppu.chr[0], dut.ppu_en, dut.ppu_x, dut.ppu_y);
    if (frames < 2) $fatal(1, "no video frames");
    if (nonblack == 0) $fatal(1, "blank framebuffer");
    $display("PASS: Open6502 RTL NROM game frame");
    $finish;
  end
endmodule
