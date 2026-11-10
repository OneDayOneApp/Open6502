// Open6502 DMC core.  This milestone implements the sample address/length,
// rate, loop, IRQ and 7-bit output.  CPU DMA stall is not included yet.
module open6502_dmc (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        cpu_en,
  input  logic        enable,
  input  logic        cs,
  input  logic        we,
  input  logic [1:0]  addr,
  input  logic [7:0]  wdata,
  input  logic [7:0]  sample_data,
  output logic [15:0] sample_addr,
  output logic [6:0]  sample_out,
  output logic        irq,
  output logic        active,
  output logic        dma_req
);
  logic        irq_enable, loop;
  logic [3:0]  rate_index;
  logic [15:0] period, timer;
  logic [15:0] start_addr, length, current_addr, bytes_left;
  logic [7:0]  buffer;
  logic        buffer_full;
  logic [7:0]  shift;
  logic [3:0]  bits;
  logic [6:0]  level;

  function automatic [15:0] rate_period(input [3:0] r);
    case (r)
      4'd0: rate_period = 16'd428; 4'd1: rate_period = 16'd380;
      4'd2: rate_period = 16'd340; 4'd3: rate_period = 16'd320;
      4'd4: rate_period = 16'd286; 4'd5: rate_period = 16'd254;
      4'd6: rate_period = 16'd226; 4'd7: rate_period = 16'd214;
      4'd8: rate_period = 16'd190; 4'd9: rate_period = 16'd160;
      4'd10: rate_period = 16'd142; 4'd11: rate_period = 16'd128;
      4'd12: rate_period = 16'd106; 4'd13: rate_period = 16'd84;
      4'd14: rate_period = 16'd72; default: rate_period = 16'd54;
    endcase
  endfunction

  assign sample_addr = current_addr;
  assign sample_out = level;
  assign active = enable && (bytes_left != 0);
  assign dma_req = enable && cpu_en && (timer == 0) && (bits == 0) &&
                   !buffer_full && (bytes_left != 0);

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      irq_enable <= 0; loop <= 0; rate_index <= 0; period <= 16'd428;
      timer <= 0; start_addr <= 16'hC000; length <= 16'd1;
      current_addr <= 16'hC000; bytes_left <= 0; buffer <= 0;
      buffer_full <= 0; shift <= 0; bits <= 0; level <= 7'd64; irq <= 0;
    end else begin
      if (cs && we) begin
        case (addr)
          2'd0: begin
            irq_enable <= wdata[7]; loop <= wdata[6]; rate_index <= wdata[3:0];
            period <= rate_period(wdata[3:0]);
            if (!wdata[7]) irq <= 1'b0;
          end
          2'd1: level <= wdata[6:0];
          2'd2: start_addr <= 16'hC000 | (wdata << 6);
          2'd3: length <= (wdata << 4) + 1;
        endcase
      end

      if (enable && bytes_left == 0 && !buffer_full) begin
        current_addr <= start_addr;
        bytes_left <= length;
        irq <= 1'b0;
      end

      if (cpu_en) begin
        if (timer == 0) begin
          timer <= period;
          if (bits == 0) begin
            if (!buffer_full && bytes_left != 0) begin
              buffer <= sample_data;
              buffer_full <= 1'b1;
              current_addr <= current_addr + 16'd1;
              bytes_left <= bytes_left - 16'd1;
            end
            if (buffer_full) begin
              shift <= buffer;
              buffer_full <= 1'b0;
              bits <= 4'd8;
            end
          end
          if (bits != 0) begin
            if (shift[0]) begin
              if (level < 7'd127) level <= level + 7'd2;
            end else begin
              if (level > 7'd0) level <= level - 7'd2;
            end
            shift <= shift >> 1;
            bits <= bits - 4'd1;
            if (bits == 4'd1) begin
              if (bytes_left == 0 && irq_enable && !loop) irq <= 1'b1;
              if (bytes_left == 0 && loop) begin
                current_addr <= start_addr;
                bytes_left <= length;
              end
            end
          end
        end else timer <= timer - 16'd1;
      end
    end
  end
endmodule
