`timescale 1ns/1ps
module open6502_clocks_tb;
  logic clk = 0, rst_n = 0;
  logic rst_sync_n, cpu_en, ppu_en, aud_en;
  int cpu_count = 0, ppu_count = 0, aud_count = 0;

  open6502_clocks dut (
    .clk_21m(clk), .rst_n(rst_n),
    .rst_sync_n(rst_sync_n), .cpu_en(cpu_en),
    .ppu_en(ppu_en), .aud_en(aud_en)
  );

  always #23.28 clk = ~clk;

  always @(posedge clk) begin
    if (rst_sync_n) begin
      if (cpu_en) cpu_count++;
      if (ppu_en) ppu_count++;
      if (aud_en) aud_count++;
    end
  end

  initial begin
    repeat (8) @(posedge clk);
    rst_n = 1;
    repeat (200000) @(posedge clk);
    if (cpu_count < 16000 || cpu_count > 17000)
      $fatal(1, "CPU enable count %0d", cpu_count);
    if (ppu_count < 49000 || ppu_count > 51000)
      $fatal(1, "PPU enable count %0d", ppu_count);
    if (aud_count < 400 || aud_count > 420)
      $fatal(1, "audio enable count %0d", aud_count);
    $display("PASS: Open6502 clocks cpu=%0d ppu=%0d aud=%0d",
             cpu_count, ppu_count, aud_count);
    $finish;
  end
endmodule
