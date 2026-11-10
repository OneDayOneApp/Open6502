module open6502_oam_dma (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        start,
  input  logic [7:0]  page,
  input  logic [7:0]  mem_rdata,
  output logic [15:0] mem_addr,
  output logic        oam_we,
  output logic [7:0]  oam_addr,
  output logic [7:0]  oam_wdata,
  output logic        busy
);
  typedef enum logic [1:0] {DMA_IDLE, DMA_READ, DMA_DONE} state_t;
  state_t state;
  logic [7:0] count;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      state <= DMA_IDLE;
      count <= 8'h00;
      mem_addr <= 16'h0000;
      oam_we <= 1'b0;
      oam_addr <= 8'h00;
      oam_wdata <= 8'h00;
      busy <= 1'b0;
    end else begin
      oam_we <= 1'b0;
      case (state)
        DMA_IDLE: begin
          if (start) begin
            mem_addr <= {page, 8'h00};
            count <= 8'h00;
            busy <= 1'b1;
            state <= DMA_READ;
          end
        end
        DMA_READ: begin
          oam_addr <= count;
          oam_wdata <= mem_rdata;
          oam_we <= 1'b1;
          mem_addr <= mem_addr + 16'd1;
          count <= count + 8'd1;
          if (count == 8'hFF) state <= DMA_DONE;
        end
        DMA_DONE: begin
          busy <= 1'b0;
          state <= DMA_IDLE;
        end
        default: state <= DMA_IDLE;
      endcase
    end
  end
endmodule
