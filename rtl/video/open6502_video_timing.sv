module open6502_video_timing (
  input  logic       clk,
  input  logic       rst_n,
  input  logic       ppu_en,
  output logic [8:0] x,
  output logic [8:0] y,
  output logic       de,
  output logic       hsync,
  output logic       vsync,
  output logic       frame_tick
);
  localparam int SCREEN_W = 256;
  localparam int SCREEN_H = 240;
  localparam int PPU_DOTS = 341;
  localparam int PPU_LINES = 262;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      x <= '0;
      y <= '0;
      frame_tick <= 1'b0;
    end else if (ppu_en) begin
      frame_tick <= 1'b0;
      if (x == PPU_DOTS - 1) begin
        x <= '0;
        if (y == PPU_LINES - 1) begin
          y <= '0;
          frame_tick <= 1'b1;
        end else begin
          y <= y + 1'b1;
        end
      end else begin
        x <= x + 1'b1;
      end
    end
  end

  assign de    = (x < SCREEN_W) && (y < SCREEN_H);
  assign hsync = ~((x >= 9'd280) && (x < 9'd304));
  assign vsync = ~((y >= 9'd241) && (y < 9'd245));
endmodule
