module open6502_clocks (
  input  logic clk_21m,
  input  logic rst_n,
  output logic rst_sync_n,
  output logic cpu_en,
  output logic ppu_en,
  output logic aud_en
);
  localparam int CPU_DIV = 12;
  localparam int PPU_DIV = 4;

  localparam logic [31:0] AUD_INC = 32'd44100;
  localparam logic [31:0] MASTER  = 32'd21477272;

  logic [3:0]  cpu_div;
  logic [1:0]  ppu_div;
  logic [31:0] aud_acc;
  logic [1:0]  rst_sync;

  always_ff @(posedge clk_21m) begin
    if (!rst_n) rst_sync <= 2'b00;
    else        rst_sync <= {rst_sync[0], 1'b1};
  end
  assign rst_sync_n = rst_sync[1];

  always_ff @(posedge clk_21m) begin
    if (!rst_sync_n) begin
      cpu_div <= 4'd0;
      ppu_div <= 2'd0;
      aud_acc <= 32'd0;
      cpu_en  <= 1'b0;
      ppu_en  <= 1'b0;
      aud_en  <= 1'b0;
    end else begin
      cpu_en <= (cpu_div == CPU_DIV - 1);
      cpu_div <= (cpu_div == CPU_DIV - 1) ? 4'd0 : cpu_div + 4'd1;

      ppu_en <= (ppu_div == PPU_DIV - 1);
      ppu_div <= (ppu_div == PPU_DIV - 1) ? 2'd0 : ppu_div + 2'd1;

      aud_en <= 1'b0;
      aud_acc <= aud_acc + AUD_INC;
      if (aud_acc + AUD_INC >= MASTER) begin
        aud_acc <= aud_acc + AUD_INC - MASTER;
        aud_en <= 1'b1;
      end
    end
  end
endmodule
