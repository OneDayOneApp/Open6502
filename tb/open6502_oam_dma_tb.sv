`timescale 1ns/1ps
module open6502_oam_dma_tb;
  logic clk = 0, rst_n = 0, start = 0;
  logic [7:0] page = 8'h02, mem_rdata;
  logic [15:0] mem_addr;
  logic oam_we;
  logic [7:0] oam_addr, oam_wdata;
  logic busy;
  logic [7:0] mem [0:65535];
  logic [7:0] oam [0:255];

  open6502_oam_dma dut (
    .clk(clk), .rst_n(rst_n), .start(start), .page(page),
    .mem_rdata(mem_rdata), .mem_addr(mem_addr),
    .oam_we(oam_we), .oam_addr(oam_addr), .oam_wdata(oam_wdata),
    .busy(busy)
  );

  always #10 clk = ~clk;
  assign mem_rdata = mem[mem_addr];

  always @(posedge clk) if (oam_we) oam[oam_addr] <= oam_wdata;

  initial begin
    for (int i = 0; i < 65536; i++) mem[i] = i[7:0];
    repeat (4) @(posedge clk); rst_n = 1;
    start = 1; @(posedge clk); #1; start = 0;
    wait (!busy);
    if (oam[0] !== 8'h00 || oam[255] !== 8'hFF)
      $fatal(1, "OAM DMA mismatch %02x %02x", oam[0], oam[255]);
    $display("PASS: Open6502 OAM DMA");
    $finish;
  end
endmodule
