`timescale 1ns/1ps
module open6502_ppu_sprite_tb;
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
    // sprite CHR tile 0 = $80 (pixel color 1)
    wr(3'd6, 8'h10); wr(3'd6, 8'h00); wr(3'd7, 8'h80);
    wr(3'd0, 8'h08); // sprite pattern table $1000
    // OAM: sprite 0 y=0 (top=1), tile 0, attr 0, x=0
    wr(3'd3, 8'h00); wr(3'd4, 8'h00); wr(3'd4, 8'h00); wr(3'd4, 8'h00); wr(3'd4, 8'h00);
    // sprite palette 1 entry 1 = $16
    wr(3'd6, 8'h3F); wr(3'd6, 8'h11); wr(3'd7, 8'h16);
    wr(3'd1, 8'h10); // sprites on
    x = 0; y = 1; #1;
    if (pixel !== 8'h16) $fatal(1, "sprite pixel=%02x", pixel);
    $display("PASS: Open6502 PPU sprite pixel");
    $finish;
  end
endmodule
