`timescale 1ns/1ps
module open6502_oam_soc_tb;
  logic clk = 0, rst_n = 0;
  logic vid_pclk, vid_de, vid_hsync, vid_vsync;
  logic [11:0] vid_rgb;
  logic aud_valid;
  logic signed [15:0] aud_sample;
  logic [15:0] cart_addr;
  logic [7:0] cart_wdata, cart_rdata;
  logic cart_we, cart_irq;
  logic dbg_valid;
  logic [63:0] dbg_payload;
  logic [7:0] cart_mem [0:65535];

  open6502_top dut (
    .clk_21m(clk), .rst_n(rst_n),
    .vid_pclk(vid_pclk), .vid_de(vid_de), .vid_hsync(vid_hsync),
    .vid_vsync(vid_vsync), .vid_rgb(vid_rgb),
    .aud_valid(aud_valid), .aud_sample(aud_sample),
    .joy1(8'h00), .joy2(8'h00),
    .cart_addr(cart_addr), .cart_wdata(cart_wdata),
    .cart_rdata(cart_rdata), .cart_we(cart_we), .cart_irq(1'b0),
    .dbg_valid(dbg_valid), .dbg_payload(dbg_payload)
  );

  always #23.28 clk = ~clk;
  assign cart_rdata = cart_addr >= 16'h8000 ? cart_mem[cart_addr] : 8'h00;

  initial begin
    for (int i = 0; i < 65536; i++) cart_mem[i] = 8'hEA;
    cart_mem[16'hFFFC] = 8'h00; cart_mem[16'hFFFD] = 8'h80;
    cart_mem[16'h8000] = 8'hA9; cart_mem[16'h8001] = 8'hAA; // LDA #$AA
    cart_mem[16'h8002] = 8'h8D; cart_mem[16'h8003] = 8'h00; cart_mem[16'h8004] = 8'h02; // STA $0200
    cart_mem[16'h8005] = 8'hA9; cart_mem[16'h8006] = 8'h02; // LDA #$02
    cart_mem[16'h8007] = 8'h8D; cart_mem[16'h8008] = 8'h14; cart_mem[16'h8009] = 8'h40; // STA $4014
    cart_mem[16'h800A] = 8'h4C; cart_mem[16'h800B] = 8'h0A; cart_mem[16'h800C] = 8'h80;
    repeat (8) @(posedge clk);
    rst_n = 1;
    repeat (3000) @(posedge clk);
    if (dut.u_ppu.oam[0] !== 8'hAA)
      $fatal(1, "OAM DMA SoC failed OAM[0]=%02x", dut.u_ppu.oam[0]);
    $display("PASS: Open6502 SoC OAM DMA");
    $finish;
  end
endmodule
