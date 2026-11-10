`timescale 1ns/1ps
module open6502_mapper_tb;
  logic clk = 0, rst_n = 0;
  logic [2:0] mapper = 0;
  logic [15:0] addr = 0;
  logic we = 0;
  logic [7:0] wdata = 0;
  logic [14:0] prg;
  logic [12:0] chr;
  logic mirror;
  logic irq;
  logic a12 = 0;

  open6502_mapper dut (
    .clk(clk), .rst_n(rst_n), .mapper(mapper), .cpu_addr(addr),
    .cpu_we(we), .cpu_wdata(wdata), .prg_addr(prg),
    .ppu_a12(a12), .chr_addr(chr), .mirroring(mirror), .irq(irq)
  );

  always #10 clk = ~clk;
  task automatic wr(input [15:0] a, input [7:0] d);
    addr = a; wdata = d; we = 1;
    @(posedge clk); #1; we = 0;
  endtask

  initial begin
    repeat (4) @(posedge clk); rst_n = 1;
    // NROM
    mapper = 0; addr = 16'h8000; #1;
    if (prg !== 15'h0000) $fatal(1, "NROM prg");
    // UxROM bank 2, lower window
    mapper = 2; wr(16'h8000, 8'h02); addr = 16'h8000; #1;
    if (prg !== 15'h2000) $fatal(1, "UxROM bank %04x", prg);
    addr = 16'hC000; #1;
    if (prg !== 15'h4000) $fatal(1, "UxROM fixed");
    // CNROM CHR bank 1
    mapper = 3; wr(16'h8000, 8'h01); addr = 16'h0000; #1;
    if (chr !== 13'h0800) $fatal(1, "CNROM chr %04x", chr);
    // AxROM bank 1 + one-screen
    mapper = 7; wr(16'h8000, 8'h11); addr = 16'h8000; #1;
    if (prg !== 15'h1000 || !mirror) $fatal(1, "AxROM prg/mirror");
    // MMC1: write 5 bits, register select via A14:A13
    mapper = 1;
    for (int b = 0; b < 5; b++) wr(16'h8000, 8'h00); // PRG bank 0
    // control = 0x0C (PRG 32K, mirroring 0)
    for (int b = 0; b < 5; b++) wr(16'h8000, (8'h0C >> b) & 1);
    addr = 16'h8000; #1;
    if (prg !== 15'h0000) $fatal(1, "MMC1 prg");
    // MMC3 PRG bank 1 and IRQ
    mapper = 4;
    wr(16'h8000, 8'h06); wr(16'h8001, 8'h01);
    addr = 16'h8000; #1;
    if (prg !== 15'h2000) $fatal(1, "MMC3 prg %04x", prg);
    wr(16'hC000, 8'h01); // latch
    wr(16'hC001, 8'h00); // reload
    wr(16'hE001, 8'h00); // enable
    a12 = 1; @(posedge clk); #1; a12 = 0;
    if (!irq) $fatal(1, "MMC3 IRQ not asserted");
    $display("PASS: Open6502 mapper 0/1/2/3/4/7");
    $finish;
  end
endmodule
