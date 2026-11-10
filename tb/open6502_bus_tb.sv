`timescale 1ns/1ps
module open6502_bus_tb;
  logic clk = 0, rst_n = 0;
  logic [15:0] addr = 0;
  logic we = 0;
  logic [7:0] wdata = 0;
  logic [7:0] ram_rdata, ppu_rdata = 8'h5A, apu_rdata = 8'hC3, cart_rdata = 8'hA5;
  logic ram_sel, ppu_sel, apu_sel, cart_sel;
  logic ram_we, ppu_we, apu_we, cart_we;
  logic [10:0] ram_addr;
  logic [7:0] rdata;

  open6502_bus u_bus (
    .cpu_addr(addr), .cpu_we(we), .cpu_wdata(wdata),
    .ram_rdata(ram_rdata), .ppu_rdata(ppu_rdata),
    .apu_rdata(apu_rdata), .cart_rdata(cart_rdata),
    .ram_sel(ram_sel), .ppu_sel(ppu_sel),
    .apu_sel(apu_sel), .cart_sel(cart_sel),
    .ram_we(ram_we), .ppu_we(ppu_we),
    .apu_we(apu_we), .cart_we(cart_we),
    .ram_addr(ram_addr), .cpu_rdata(rdata)
  );

  open6502_wram u_ram (
    .clk(clk), .rst_n(rst_n), .we(ram_we),
    .addr(ram_addr), .wdata(wdata), .rdata(ram_rdata)
  );

  always #23.28 clk = ~clk;

  initial begin
    repeat (4) @(posedge clk);
    rst_n = 1;
    // WRAM mirror write/read
    addr = 16'h0000; wdata = 8'h12; we = 1; @(posedge clk); we = 0;
    addr = 16'h0800; #1;
    if (rdata !== 8'h12) $fatal(1, "WRAM mirror failed");
    // PPU / APU / cart selects
    addr = 16'h2000; #1;
    if (!ppu_sel || ppu_we) $fatal(1, "PPU decode failed");
    addr = 16'h4000; #1;
    if (!apu_sel) $fatal(1, "APU decode failed");
    addr = 16'h8000; #1;
    if (!cart_sel) $fatal(1, "cart decode failed");
    if (rdata !== 8'hA5) $fatal(1, "cart read failed");
    $display("PASS: Open6502 bus decode + WRAM mirror");
    $finish;
  end
endmodule
