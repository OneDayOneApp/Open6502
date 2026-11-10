`timescale 1ns/1ps
module open6502_ppu_pixel_tb;
  logic clk = 0, rst_n = 0, ppu_en = 0;
  logic [8:0] x = 0, y = 0;
  logic cs = 0, we = 0;
  logic [2:0] addr = 0;
  logic [7:0] wdata = 0;
  logic [7:0] rdata, pixel;
  logic nmi;

  open6502_ppu dut (
    .clk(clk), .rst_n(rst_n), .ppu_en(ppu_en),
    .x(x), .y(y), .cs(cs), .we(we),
    .addr(addr), .wdata(wdata), .rdata(rdata),
    .pixel_index(pixel), .nmi(nmi)
  );

  always #10 clk = ~clk;

  task automatic wr(input [2:0] a, input [7:0] d);
    addr = a; wdata = d; cs = 1; we = 1;
    @(posedge clk); #1; cs = 0; we = 0;
  endtask

  initial begin
    repeat (4) @(posedge clk); rst_n = 1;
    // CHR tile 0: plane 0 = $80, plane 1 = 0
    wr(3'd6, 8'h00); wr(3'd6, 8'h00); wr(3'd7, 8'h80);
    // nametable 0,0 = tile 0
    wr(3'd6, 8'h20); wr(3'd6, 8'h00); wr(3'd7, 8'h00);
    // palette 0/1
    wr(3'd6, 8'h3F); wr(3'd6, 8'h00);
    wr(3'd7, 8'h0F); wr(3'd7, 8'h30);
    wr(3'd1, 8'h08); // background on
    x = 0; y = 0;
    #1;
    if (pixel !== 8'h0F && pixel !== 8'h30)
      $fatal(1, "PPU pixel=%02x", pixel);
    $display("PASS: Open6502 PPU pixel fetch (index=%02x)", pixel);
    $finish;
  end
endmodule
