`timescale 1ns/1ps
module open6502_ppu_tb;
  logic clk = 0, rst_n = 0, ppu_en = 0;
  logic [8:0] x = 0, y = 0;
  logic cs = 0, we = 0;
  logic [2:0] addr = 0;
  logic [7:0] wdata = 0;
  logic [7:0] rdata;
  logic nmi;

  open6502_ppu dut (
    .clk(clk), .rst_n(rst_n), .ppu_en(ppu_en),
    .x(x), .y(y), .cs(cs), .we(we),
    .addr(addr), .wdata(wdata), .rdata(rdata), .nmi(nmi)
  );

  always #10 clk = ~clk;

  task automatic wr(input [2:0] a, input [7:0] d);
    addr = a; wdata = d; cs = 1; we = 1;
    @(posedge clk); cs = 0; we = 0;
  endtask

  initial begin
    repeat (4) @(posedge clk); rst_n = 1;
    wr(3'd0, 8'h80);           // NMI on
    x = 9'd1; y = 9'd241; ppu_en = 1;
    @(posedge clk); #1; ppu_en = 0;
    if (!nmi) $fatal(1, "vblank NMI not asserted");
    addr = 3'd2; cs = 1; #1;
    if (!rdata[7]) $fatal(1, "vblank status not set");
    @(posedge clk); #1; cs = 0;
    if (nmi) $fatal(1, "NMI not cleared by $2002 read");
    // $2006/$2007 write then read
    wr(3'd6, 8'h20); wr(3'd6, 8'h00);
    wr(3'd7, 8'hA5);
    wr(3'd6, 8'h20); wr(3'd6, 8'h00);
    addr = 3'd7; cs = 1; #1; @(posedge clk); cs = 0;
    $display("PASS: Open6502 PPU registers/vblank");
    $finish;
  end
endmodule
