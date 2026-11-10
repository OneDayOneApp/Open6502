`timescale 1ns/1ps
module open6502_apu_sweep_tb;
  logic clk=0,rst_n=0,cpu_en=0,cs=0,we=0; logic [4:0] addr=0; logic [7:0] wdata=0,rdata;
  logic signed [15:0] sample; logic irq;
  open6502_apu dut(.clk(clk),.rst_n(rst_n),.cpu_en(cpu_en),.cs(cs),.we(we),.addr(addr),.wdata(wdata),.rdata(rdata),.sample(sample),.irq(irq));
  always #10 clk=~clk;
  task automatic wr(input [4:0] a,input [7:0] d); addr=a;wdata=d;cs=1;we=1;@(posedge clk);#1;cs=0;we=0; endtask
  initial begin
    repeat(4)@(posedge clk); rst_n=1;
    wr(5'h15,8'h01); wr(5'h01,8'h81); wr(5'h02,8'h80); wr(5'h03,8'h01);
    repeat(8000) begin @(posedge clk); cpu_en=1; @(posedge clk); #1; cpu_en=0; end
    if (dut.p1_period == 11'h180)
      $fatal(1, "APU pulse sweep did not update period");
    $display("PASS: Open6502 APU sweep period=%03x", dut.p1_period);
    $finish;
  end
endmodule
