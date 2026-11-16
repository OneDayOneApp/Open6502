`timescale 1ns/1ps
// am6502 flat-image DiffTest bench: run a 64 KiB memory image on the CPU and
// print one line per retired instruction, in the same format NEMU emits via
// NEMU_TRACE_STATE:
//
//   vvp flat_tb.vvp +IMAGE=app.hex +TRACE=rtl.trace [+MAX_INST=N] [+STOP_PC=0x3469]
//
// The image is loaded as-is and boots from its own reset vector: no patching.
// The run stops after the instruction that writes the halt port ($F002), after
// reaching +STOP_PC, or when +MAX_INST is exhausted.
module open6502_am6502_flat_tb;
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
  always @(posedge clk) if (rst_n && we) mem[addr] <= wdata;

  string image_path, trace_path;
  int max_inst = 2_000_000;
  int stop_pc_arg = -1;
  int inst = 0, cycles = 0;
  int fd = 0;
  bit started = 0, halted = 0, done = 0;

  always @(posedge clk) begin
    cycles <= cycles + 1;
    if (rst_n && cpu_en && we && addr == 16'hF002) halted <= 1;
  end

  // One state line per retired instruction: sampling at the fetch boundary
  // gives the post-retire PC/registers, which is what NEMU's trace records.
  always @(posedge clk) begin
    if (rst_n && cpu_en && dut.state == dut.ST_FETCH) begin
      if (started) begin
        if (fd != 0) $fdisplay(fd, "%04x %02x %02x %02x %02x %02x", pc, a, x, y, sp, p);
        inst <= inst + 1;
        if (halted || (stop_pc_arg >= 0 && pc == stop_pc_arg[15:0]) || inst + 1 >= max_inst)
          done <= 1;
      end else begin
        started <= 1;   // the first fetch is the pre-first state; NEMU does not log it
      end
    end
  end

  initial begin
    if (!$value$plusargs("IMAGE=%s", image_path))
      $fatal(1, "FAIL: +IMAGE=<hex> is required");
    if (!$value$plusargs("TRACE=%s", trace_path))
      $fatal(1, "FAIL: +TRACE=<path> is required");
    void'($value$plusargs("MAX_INST=%d", max_inst));
    void'($value$plusargs("STOP_PC=%h", stop_pc_arg));

    $readmemh(image_path, mem);
    fd = $fopen(trace_path, "w");
    if (fd == 0) $fatal(1, "FAIL: cannot open %s", trace_path);

    repeat (4) @(posedge clk);
    rst_n = 1;
    cpu_en = 1;

    while (!done && cycles < max_inst * 16 + 10000) @(posedge clk);
    @(posedge clk);
    if (fd != 0) $fclose(fd);

    $display("AM6502 FLAT: inst=%0d halted=%b stop_pc=%04x pc=%04x cycles=%0d",
             inst, halted, stop_pc_arg[15:0], pc, cycles);
    if (!done) $fatal(1, "FAIL: flat DiffTest run did not finish");
    $finish;
  end
endmodule
