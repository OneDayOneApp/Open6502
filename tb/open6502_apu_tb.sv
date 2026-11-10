`timescale 1ns/1ps
module open6502_apu_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0, cs = 0, we = 0;
  logic [4:0] addr = 0;
  logic [7:0] wdata = 0, rdata;
  logic signed [15:0] sample;
  logic irq;

  open6502_apu dut (
    .clk(clk), .rst_n(rst_n), .cpu_en(cpu_en),
    .cs(cs), .we(we), .addr(addr), .wdata(wdata),
    .rdata(rdata), .sample(sample), .irq(irq)
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
    // triangle + noise
    wr(5'h15, 8'h0C);
    wr(5'h08, 8'hFF);
    wr(5'h0A, 8'hFD); wr(5'h0B, 8'h00);
    wr(5'h0C, 8'h0F);
    wr(5'h0E, 8'h00); wr(5'h0F, 8'h00);
    repeat (200) begin @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0; end
    addr = 5'h15; cs = 1; #1;
    if (!rdata[2] || !rdata[3]) $fatal(1, "triangle/noise status missing");
    cs = 0;
    if (sample == 0) $fatal(1, "triangle/noise sample silent");
    // frame IRQ: 4-step mode, IRQ enabled
    wr(5'h17, 8'h00);
    repeat (30000) begin @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0; end
    if (!irq) $fatal(1, "APU frame IRQ not asserted");
    addr = 5'h15; cs = 1; #1;
    if (!rdata[6]) $fatal(1, "APU frame IRQ status missing");
    @(posedge clk); #1; cs = 0;
    if (irq) $fatal(1, "APU frame IRQ not cleared by $4015");
    $display("PASS: Open6502 APU pulse/triangle/noise/frame IRQ");
    $finish;
  end
endmodule
