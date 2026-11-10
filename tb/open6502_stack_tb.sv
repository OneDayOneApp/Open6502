`timescale 1ns/1ps
module open6502_stack_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0;
  logic [7:0] mem [0:65535];
  logic [7:0] rdata;
  logic [15:0] addr;
  logic [7:0] wdata;
  logic we;
  logic [15:0] pc;
  logic [7:0] a, x, y, sp, p;

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
    for (int i = 0; i < 65536; i++) mem[i] = 8'hEA;
    mem[16'hFFFC] = 8'h00; mem[16'hFFFD] = 8'h80;
    mem[16'h8000] = 8'hA9; mem[16'h8001] = 8'h42; // LDA #$42
    mem[16'h8002] = 8'h48;                         // PHA
    mem[16'h8003] = 8'hA9; mem[16'h8004] = 8'h00; // LDA #$00
    mem[16'h8005] = 8'h68;                         // PLA
    mem[16'h8006] = 8'h8D; mem[16'h8007] = 8'h00; mem[16'h8008] = 8'h02; // STA $0200
    mem[16'h8009] = 8'h4C; mem[16'h800A] = 8'h09; mem[16'h800B] = 8'h80;
    repeat (4) @(posedge clk);
    rst_n = 1;
    repeat (100) begin
      @(posedge clk); cpu_en = 1; @(posedge clk); cpu_en = 0;
    end
    if (a !== 8'h42) $fatal(1, "PLA failed A=%02x", a);
    if (mem[16'h0200] !== 8'h42) $fatal(1, "PLA/STA failed mem=%02x", mem[16'h0200]);
    if (sp !== 8'hFD) $fatal(1, "SP not restored: %02x", sp);
    $display("PASS: Open6502 CPU PHA/PLA stack");
    $finish;
  end
endmodule
