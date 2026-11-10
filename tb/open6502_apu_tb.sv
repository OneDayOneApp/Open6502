`timescale 1ns/1ps
module open6502_apu_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0, cs = 0, we = 0;
  logic [4:0] addr = 0;
  logic [7:0] wdata = 0, rdata;
  logic signed [15:0] sample;

  open6502_apu dut (
    .clk(clk), .rst_n(rst_n), .cpu_en(cpu_en),
    .cs(cs), .we(we), .addr(addr), .wdata(wdata),
    .rdata(rdata), .sample(sample)
  );

  always #10 clk = ~clk;
  task automatic wr(input [4:0] a, input [7:0] d);
    addr = a; wdata = d; cs = 1; we = 1;
    @(posedge clk); #1; cs = 0; we = 0;
  endtask

  initial begin
    repeat (4) @(posedge clk); rst_n = 1;
    wr(5'h15, 8'h03);
    wr(5'h00, 8'hBF); // pulse 1, duty 2, constant volume 15
    wr(5'h02, 8'hFD);
    wr(5'h03, 8'h00);
    repeat (100) begin @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0; end
    addr = 5'h15; cs = 1; #1;
    if (!rdata[0]) $fatal(1, "pulse 1 length status not set");
    cs = 0;
    if (sample == 0) $fatal(1, "APU sample silent");
    $display("PASS: Open6502 APU pulse/status");
    $finish;
  end
endmodule
