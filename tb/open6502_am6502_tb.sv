`timescale 1ns/1ps
// am6502 cartridge bench: run an .nes PRG/CHR pair on the RTL SoC and decide
// PASS/FAIL the same way NEMU and GEM6502 do.
//
//   vvp am6502_tb.vvp +PRG=game.prg.hex +CHR=game.chr.hex [+MAX_CYCLES=N]
//
// The cartridge's own reset vector boots the CPU (no bench patching), $4018
// writes go to the console, and a write to $4019 with code 0 is the pass
// condition.  A blank framebuffer or a short run fails.
module open6502_am6502_tb;
  localparam int PRG_WORDS = 32768;
  localparam int CHR_WORDS = 8192;

  string prg_path, chr_path;
  int max_cycles = 3_000_000;

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

  int cycles = 0, pixels = 0, nonblack = 0, frames = 0;
  logic vsync_d = 1;
  logic [7:0] halt_code = 0;
  logic halt_seen = 0;
  // The CPU holds bus_we for several clocks per store, so the host ports are
  // edge-detected: one character per write, not one per clock.
  logic putc_d = 0, halt_wr_d = 0;

  always @(posedge clk) begin
    cycles = cycles + 1;
    putc_d <= dut.u_cpu.bus_we && (dut.u_cpu.bus_addr == 16'h4018);
    halt_wr_d <= dut.u_cpu.bus_we && (dut.u_cpu.bus_addr == 16'h4019);
    if (dut.u_cpu.bus_we && !putc_d && dut.u_cpu.bus_addr == 16'h4018)
      $write("%c", dut.u_cpu.bus_wdata);
    if (dut.u_cpu.bus_we && !halt_wr_d && dut.u_cpu.bus_addr == 16'h4019) begin
      halt_code = dut.u_cpu.bus_wdata;
      halt_seen = 1;
    end
    if (vid_de) begin
      pixels = pixels + 1;
      if (vid_rgb != 0) nonblack = nonblack + 1;
    end
    if (vid_vsync !== vsync_d) begin
      vsync_d = vid_vsync;
      if (!vid_vsync) frames = frames + 1;
    end
  end

  initial begin
    if (!$value$plusargs("PRG=%s", prg_path)) begin
      $fatal(1, "FAIL: +PRG=<prg.hex> is required");
    end
    if (!$value$plusargs("CHR=%s", chr_path)) begin
      $fatal(1, "FAIL: +CHR=<chr.hex> is required");
    end
    void'($value$plusargs("MAX_CYCLES=%d", max_cycles));
    $readmemh(prg_path, prg);
    $readmemh(chr_path, chr);

    repeat (8) @(posedge clk);
    rst_n = 1;

    // CHR RAM is loaded from the cartridge's CHR-ROM at power-on.
    for (int i = 0; i < CHR_WORDS; i++) begin
      @(posedge clk);
      chr_load_addr = i[12:0];
      chr_load_data = chr[i];
      chr_load_we = 1;
    end
    @(posedge clk); chr_load_we = 0;

    // Run until the image halts and has had a frame or two to be visible.
    while ((!halt_seen || frames < 3) && cycles < max_cycles) @(posedge clk);
    $display("");

    $display("AM6502: halt=%02x frames=%0d nonblack=%0d pixels=%0d cycles=%0d",
             halt_code, frames, nonblack, pixels, cycles);
    if (!halt_seen) $fatal(1, "FAIL: no $4019 halt within %0d cycles", max_cycles);
    if (halt_code != 8'h00) $fatal(1, "FAIL: halt code %02x", halt_code);
    if (frames < 2) $fatal(1, "FAIL: only %0d frames rendered", frames);
    if (nonblack == 0) $fatal(1, "FAIL: blank framebuffer");
    $display("PASS: am6502 on RTL (halt 0, %0d frames, %0d nonblack pixels)",
             frames, nonblack);
    $finish;
  end
endmodule
