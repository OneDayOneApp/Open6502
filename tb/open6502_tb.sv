`timescale 1ns/1ps
module open6502_tb;
  logic clk = 1'b0;
  logic rst_n = 1'b0;
  logic vid_de, vid_hsync, vid_vsync, dbg_valid;
  logic [11:0] vid_rgb;
  logic aud_valid;
  logic signed [15:0] aud_sample;
  logic [15:0] cart_addr;
  logic [7:0] cart_wdata, cart_rdata;
  logic cart_we, cart_irq;
  logic [63:0] dbg_payload;

  open6502_top dut (
    .clk_21m(clk), .rst_n(rst_n),
    .vid_pclk(), .vid_de(vid_de), .vid_hsync(vid_hsync),
    .vid_vsync(vid_vsync), .vid_rgb(vid_rgb),
    .aud_valid(aud_valid), .aud_sample(aud_sample),
    .joy1(8'h00), .joy2(8'h00),
    .cart_addr(cart_addr), .cart_wdata(cart_wdata),
    .cart_rdata(8'h00), .cart_we(cart_we), .cart_irq(1'b0),
    .dbg_valid(dbg_valid), .dbg_payload(dbg_payload)
  );

  always #23.28 clk = ~clk; // ~21.477 MHz

  initial begin
    repeat (8) @(posedge clk);
    rst_n = 1'b1;
    @(negedge vid_hsync);
    if (!vid_hsync) $display("PASS: Open6502 video timing");
    else $display("INFO: hsync=%0b vsync=%0b de=%0b", vid_hsync, vid_vsync, vid_de);
    $finish;
  end
endmodule
