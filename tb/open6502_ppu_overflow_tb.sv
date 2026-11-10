`timescale 1ns/1ps
module open6502_ppu_overflow_tb;
  logic clk = 0, rst_n = 0, ppu_en = 0;
  logic [8:0] x = 0, y = 0;
  logic cs = 0, we = 0;
  logic [2:0] addr = 0;
  logic [7:0] wdata = 0;
  logic [7:0] rdata, pixel;
  logic s0, nmi;

  open6502_ppu dut (
    .clk(clk), .rst_n(rst_n), .ppu_en(ppu_en),
    .x(x), .y(y), .cs(cs), .we(we),
    .addr(addr), .wdata(wdata), .rdata(rdata),
    .pixel_index(pixel), .sprite0_hit(s0), .nmi(nmi)
  );

  always #10 clk = ~clk;
  task automatic wr(input [2:0] a, input [7:0] d);
    addr = a; wdata = d; cs = 1; we = 1;
    @(posedge clk); #1; cs = 0; we = 0;
  endtask

  initial begin
    repeat (4) @(posedge clk); rst_n = 1;
    wr(3'd1, 8'h10); // sprites on
    wr(3'd3, 8'h00);
    for (int i = 0; i < 9; i++) begin
      wr(3'd4, 8'h00); // y
      wr(3'd4, 8'h00); // tile
      wr(3'd4, 8'h00); // attr
      wr(3'd4, 8'h00); // x
    end
    x = 0; y = 1; ppu_en = 1;
    @(posedge clk); #1; ppu_en = 0;
    addr = 3'd2; cs = 1; #1;
    if (!rdata[5]) $fatal(1, "sprite overflow not set");
    $display("PASS: Open6502 PPU sprite overflow");
    $finish;
  end
endmodule
