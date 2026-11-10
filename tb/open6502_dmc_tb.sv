`timescale 1ns/1ps
module open6502_dmc_tb;
  logic clk=0,rst_n=0,cpu_en=0,enable=0,cs=0,we=0;
  logic [1:0] addr=0; logic [7:0] wdata=0, sample_data=8'hAA;
  logic [15:0] sample_addr; logic [6:0] sample_out; logic irq, active;
  logic saw_level = 0;

  open6502_dmc dut(.clk(clk),.rst_n(rst_n),.cpu_en(cpu_en),.enable(enable),
    .cs(cs),.we(we),.addr(addr),.wdata(wdata),.sample_data(sample_data),
    .sample_addr(sample_addr),.sample_out(sample_out),.irq(irq),.active(active));

  always #10 clk=~clk;
  task automatic wr(input [1:0] a,input [7:0] d);
    addr=a;wdata=d;cs=1;we=1;@(posedge clk);#1;cs=0;we=0;
  endtask

  initial begin
    repeat(4)@(posedge clk); rst_n=1;
    wr(2'd0,8'h80); // IRQ enable, rate 0
    wr(2'd2,8'h00); // $C000
    wr(2'd3,8'h00); // 1 byte
    enable=1;
    repeat(2000) begin
      @(posedge clk); cpu_en=1; @(posedge clk); #1; cpu_en=0;
      if (dut.level != 7'd64) saw_level = 1;
    end
    if (dut.bits == 0) $fatal(1,"DMC did not load sample bits");
    if (!saw_level) $fatal(1,"DMC output level never changed");
    $display("PASS: Open6502 DMC sample fetch/output");
    $finish;
  end
endmodule
