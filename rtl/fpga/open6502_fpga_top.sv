// FPGA board wrapper for the Open6502 SoC.
//
// The board supplies a 21.477272 MHz clock (PLL), reset, controller inputs and
// cartridge memory. This wrapper exposes VGA, a 1-bit sigma-delta audio pin and
// the cartridge bus. It is board-agnostic; pin constraints live outside this
// repository.
module open6502_fpga_top (
  input  logic        clk_21m,
  input  logic        rst_n,

  input  logic [7:0]  joy1,
  input  logic [7:0]  joy2,

  output logic [15:0] cart_addr,
  output logic [7:0]  cart_wdata,
  input  logic [7:0]  cart_rdata,
  output logic        cart_we,
  input  logic        cart_irq,

  output logic        vga_hsync,
  output logic        vga_vsync,
  output logic [11:0] vga_rgb,
  output logic        aud_pwm,
  output logic        vid_de
);
  logic        vid_pclk;
  logic        aud_valid;
  logic signed [15:0] aud_sample;
  logic        dbg_valid;
  logic [63:0] dbg_payload;
  logic [16:0] audio_acc;

  open6502_top u_soc (
    .clk_21m(clk_21m), .rst_n(rst_n),
    .vid_pclk(vid_pclk), .vid_de(vid_de),
    .vid_hsync(vga_hsync), .vid_vsync(vga_vsync), .vid_rgb(vga_rgb),
    .aud_valid(aud_valid), .aud_sample(aud_sample),
    .joy1(joy1), .joy2(joy2),
    .cart_addr(cart_addr), .cart_wdata(cart_wdata),
    .cart_rdata(cart_rdata), .cart_we(cart_we), .cart_irq(cart_irq),
    .dbg_valid(dbg_valid), .dbg_payload(dbg_payload)
  );

  // First-order sigma-delta audio DAC.
  always_ff @(posedge clk_21m) begin
    if (!rst_n) audio_acc <= 17'd0;
    else if (aud_valid) audio_acc <= audio_acc[15:0] + (aud_sample + 17'sd32768);
  end

  assign aud_pwm = audio_acc[16];
endmodule
