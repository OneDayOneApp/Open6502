`timescale 1ns/1ps
module open6502_cpu_difftest_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0;
  logic [7:0] mem [0:65535];
  logic [7:0] rdata;
  logic [15:0] addr;
  logic [7:0] wdata;
  logic we;
  logic [15:0] pc;
  logic [7:0] a, x, y, sp, p;
  logic [15:0] trace_pc [0:31];
  int trace_count = 0;
  logic [15:0] stop_pc = 16'h8008;

  open6502_cpu dut (
    .clk(clk), .rst_n(rst_n), .cpu_en(cpu_en),
    .bus_rdata(rdata), .irq_n(1'b1), .nmi_n(1'b1),
    .bus_addr(addr), .bus_wdata(wdata), .bus_we(we),
    .dbg_pc(pc), .dbg_a(a), .dbg_x(x), .dbg_y(y), .dbg_sp(sp), .dbg_p(p)
  );

  always #10 clk = ~clk;
  assign rdata = mem[addr];
  always @(posedge clk) if (rst_n && cpu_en && we) mem[addr] <= wdata;

  always @(posedge clk) begin
    if (rst_n && cpu_en && dut.state == dut.ST_FETCH && trace_count < 32) begin
      trace_pc[trace_count] = pc;
      trace_count++;
    end
  end

  initial begin
    $readmemh("difftest.hex", mem);
    void'($value$plusargs("STOP=%h", stop_pc));
    repeat (4) @(posedge clk);
    rst_n = 1;
    for (int i = 0; i < 2000; i++) begin
      @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0;
      if (pc == stop_pc && (stop_pc != 16'h8008 || x == 8'h04)) begin
        repeat (8) begin
          @(posedge clk); cpu_en = 1; @(posedge clk); #1; cpu_en = 0;
        end
        $display("RTL A=%02X X=%02X Y=%02X SP=%02X P=%02X PC=%04X",
                 a, x, y, sp, p, pc);
        for (int t = 0; t < trace_count; t++)
          $display("PC %04X", trace_pc[t]);
        $finish;
      end
    end
    $fatal(1, "RTL timeout PC=%04x", pc);
  end
endmodule
