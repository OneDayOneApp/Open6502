`timescale 1ns/1ps
module open6502_zp37_tb;
  logic clk=0,rst_n=0,cpu_en=0; logic [7:0] mem[0:65535]; logic [7:0] rdata;
  logic [15:0] addr; logic [7:0] wdata; logic we; logic [15:0] pc;
  logic [7:0] a,x,y,sp,p;
  open6502_cpu dut(.clk(clk),.rst_n(rst_n),.cpu_en(cpu_en),.bus_rdata(rdata),
    .irq_n(1'b1),.nmi_n(1'b1),.bus_addr(addr),.bus_wdata(wdata),.bus_we(we),
    .dbg_pc(pc),.dbg_a(a),.dbg_x(x),.dbg_y(y),.dbg_sp(sp),.dbg_p(p),
    .dbg_state());
  always #10 clk=~clk;
  assign rdata=mem[addr];
  always @(posedge clk) if(rst_n&&cpu_en&&we) mem[addr]<=wdata;
  initial begin
    for(int i=0;i<65536;i++) mem[i]=8'hEA;
    mem[16'hFFFC]=8'h00; mem[16'hFFFD]=8'h80;
    // LDA #$01; STA $37; LDA $37; STA $0200; LDA #$00; STA $37; LDA $37; STA $0201
    mem[16'h8000]=8'hA9; mem[16'h8001]=8'h01;
    mem[16'h8002]=8'h85; mem[16'h8003]=8'h37;
    mem[16'h8004]=8'hA5; mem[16'h8005]=8'h37;
    mem[16'h8006]=8'h8D; mem[16'h8007]=8'h00; mem[16'h8008]=8'h02;
    mem[16'h8009]=8'hA9; mem[16'h800A]=8'h00;
    mem[16'h800B]=8'h85; mem[16'h800C]=8'h37;
    mem[16'h800D]=8'hA5; mem[16'h800E]=8'h37;
    mem[16'h800F]=8'h8D; mem[16'h8010]=8'h01; mem[16'h8011]=8'h02;
    mem[16'h8012]=8'h4C; mem[16'h8013]=8'h12; mem[16'h8014]=8'h80;
    repeat(4)@(posedge clk); rst_n=1;
    repeat(60) begin @(posedge clk); cpu_en=1; @(posedge clk); #1; cpu_en=0; end
    if (mem[16'h0200] !== 8'h01) $fatal(1,"STA/LDA $37 readback[0]=%02x",mem[16'h0200]);
    if (mem[16'h0201] !== 8'h00) $fatal(1,"STA/LDA $37 readback[1]=%02x",mem[16'h0201]);
    $display("PASS: Open6502 CPU zero-page $37 roundtrip");
    $finish;
  end
endmodule
