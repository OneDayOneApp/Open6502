`timescale 1ns/1ps
module open6502_klaus_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0;
  logic [7:0] mem [0:65535];
  logic [7:0] rdata;
  logic [15:0] addr;
  logic [7:0] wdata;
  logic we;
  logic [15:0] pc;
  logic [7:0] a, x, y, sp, p;
  longint cycles = 0;

  open6502_cpu dut (
    .clk(clk), .rst_n(rst_n), .cpu_en(cpu_en),
    .bus_rdata(rdata), .irq_n(1'b1), .nmi_n(1'b1),
    .bus_addr(addr), .bus_wdata(wdata), .bus_we(we),
    .dbg_pc(pc), .dbg_a(a), .dbg_x(x), .dbg_y(y), .dbg_sp(sp), .dbg_p(p)
  );

  always #10 clk = ~clk;
  assign rdata = mem[addr];
  always @(posedge clk) if (rst_n && cpu_en && we) mem[addr] <= wdata;

  initial begin
    $readmemh("klaus.hex", mem);
    mem[16'hFFFC] = 8'h00;
    mem[16'hFFFD] = 8'h04;
    repeat (4) @(posedge clk);
    rst_n = 1;
    while (cycles < 200_000_000) begin
      @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0;
      cycles++;
      if (pc == 16'h3469) begin
        $display("PASS: Open6502 RTL Klaus PC=%04x cycles=%0d", pc, cycles);
        $finish;
      end
    end
    $display("FAIL: Open6502 RTL Klaus timeout PC=%04x cycles=%0d", pc, cycles);
    $finish;
  end
endmodule
