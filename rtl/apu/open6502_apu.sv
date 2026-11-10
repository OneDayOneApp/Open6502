// Open6502 APU: pulse channel 1/2, length/envelope, frame sequencer and
// $4015 status. Triangle/noise/DMC are added in later milestones.
module open6502_apu (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        cpu_en,
  input  logic        cs,
  input  logic        we,
  input  logic [4:0]  addr,
  input  logic [7:0]  wdata,
  output logic [7:0]  rdata,
  output logic signed [15:0] sample,
  output logic        irq
);
  logic [7:0] p1_duty, p1_vol, p1_env, p1_period_lo, p1_period_hi;
  logic [7:0] p2_duty, p2_vol, p2_env, p2_period_lo, p2_period_hi;
  logic [7:0] p1_sweep, p2_sweep;
  logic [7:0] p1_len, p2_len;
  logic [7:0] p1_timer, p2_timer;
  logic [2:0] p1_pos, p2_pos;
  logic [3:0] p1_env_div, p2_env_div;
  logic       p1_halt, p2_halt, p1_const, p2_const;
  logic       p1_en, p2_en;
  logic [15:0] frame_div;
  logic [1:0]  frame_step;
  logic        tri_ctrl, tri_en;
  logic [6:0]  tri_linear;
  logic [7:0]  tri_period_lo, tri_period_hi, tri_timer, tri_len;
  logic [4:0]  tri_pos;
  logic [7:0]  noise_period_lo, noise_period_hi, noise_timer, noise_len;
  logic        noise_en, noise_mode;
  logic [3:0]  noise_vol;
  logic [14:0] noise_lfsr;
  logic [3:0]  noise_out;
  logic [10:0] p1_period, p2_period;
  logic [10:0] p1_target, p2_target;
  logic [3:0]  p1_shift, p2_shift;
  logic        frame_inhibit, frame_mode;

  function automatic [3:0] tri_step(input [4:0] pos);
    if (pos < 5'd16) tri_step = 4'd15 - pos[3:0];
    else tri_step = pos[3:0] - 4'd0;
  endfunction

  function automatic logic duty_bit(input [1:0] d, input [2:0] pos);
    case (d)
      2'd0: duty_bit = (pos == 3'd1);
      2'd1: duty_bit = (pos == 3'd1) || (pos == 3'd2);
      2'd2: duty_bit = (pos >= 3'd1) && (pos <= 3'd4);
      default: duty_bit = (pos == 3'd0) || (pos >= 3'd4);
    endcase
  endfunction

  function automatic [7:0] length_table(input [7:0] v);
    case (v[7:3])
      5'd0: length_table = 8'd10;
      5'd1: length_table = 8'd254;
      5'd2: length_table = 8'd20;
      5'd3: length_table = 8'd2;
      5'd4: length_table = 8'd40;
      5'd5: length_table = 8'd4;
      5'd6: length_table = 8'd80;
      5'd7: length_table = 8'd6;
      5'd8: length_table = 8'd160;
      5'd9: length_table = 8'd8;
      5'd10: length_table = 8'd60;
      5'd11: length_table = 8'd10;
      5'd12: length_table = 8'd14;
      5'd13: length_table = 8'd12;
      5'd14: length_table = 8'd26;
      default: length_table = 8'd14;
    endcase
  endfunction

  always_comb begin
    rdata = 8'h00;
    if (addr == 5'h15) begin
      rdata[6] = irq;
      rdata[0] = p1_len != 0;
      rdata[1] = p2_len != 0;
      rdata[2] = tri_len != 0;
      rdata[3] = noise_len != 0;
    end
  end

  assign p1_period = {p1_period_hi[2:0], p1_period_lo};
  assign p2_period = {p2_period_hi[2:0], p2_period_lo};
  assign p1_shift = p1_sweep[2:0];
  assign p2_shift = p2_sweep[2:0];

  wire p1_out = (p1_en && p1_len != 0 && duty_bit(p1_duty[1:0], p1_pos)) ? p1_vol : 8'd0;
  wire p2_out = (p2_en && p2_len != 0 && duty_bit(p2_duty[1:0], p2_pos)) ? p2_vol : 8'd0;
  wire [3:0] tri_out = (tri_en && tri_len != 0 && tri_linear != 0) ? tri_step(tri_pos) : 4'd0;
  wire [3:0] noise_out_w = (noise_en && noise_len != 0 && !noise_lfsr[0]) ? noise_vol : 4'd0;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      p1_duty <= 0; p2_duty <= 0; p1_vol <= 0; p2_vol <= 0;
      p1_env <= 0; p2_env <= 0; p1_period_lo <= 0; p2_period_lo <= 0;
      p1_sweep <= 0; p2_sweep <= 0;
      p1_period_hi <= 0; p2_period_hi <= 0; p1_len <= 0; p2_len <= 0;
      p1_timer <= 0; p2_timer <= 0; p1_pos <= 0; p2_pos <= 0;
      p1_halt <= 0; p2_halt <= 0; p1_const <= 0; p2_const <= 0;
      p1_en <= 0; p2_en <= 0; frame_div <= 0; frame_step <= 0;
      tri_ctrl <= 0; tri_en <= 0; tri_linear <= 0; tri_period_lo <= 0;
      tri_period_hi <= 0; tri_timer <= 0; tri_len <= 0; tri_pos <= 0;
      noise_en <= 0; noise_mode <= 0; noise_vol <= 0; noise_len <= 0;
      noise_period_lo <= 0; noise_period_hi <= 0; noise_timer <= 0;
      noise_lfsr <= 15'h0001;
      sample <= 0; irq <= 1'b0;
      frame_inhibit <= 1'b0; frame_mode <= 1'b0;
    end else begin
      if (cs && !we && addr == 5'h15) irq <= 1'b0;
      if (cs && we) begin
        case (addr)
          5'h00: begin p1_duty <= wdata[7:6]; p1_halt <= wdata[5];
                       p1_const <= wdata[4]; p1_vol <= wdata[3:0]; end
          5'h01: p1_sweep <= wdata;
          5'h02: p1_period_lo <= wdata;
          5'h03: begin p1_period_hi <= wdata[2:0]; p1_len <= length_table(wdata);
                       p1_env <= 8'd15; end
          5'h04: begin p2_duty <= wdata[7:6]; p2_halt <= wdata[5];
                       p2_const <= wdata[4]; p2_vol <= wdata[3:0]; end
          5'h05: p2_sweep <= wdata;
          5'h06: p2_period_lo <= wdata;
          5'h07: begin p2_period_hi <= wdata[2:0]; p2_len <= length_table(wdata);
                       p2_env <= 8'd15; end
          5'h08: begin tri_ctrl <= wdata[7]; tri_linear <= wdata[6:0]; end
          5'h0A: tri_period_lo <= wdata;
          5'h0B: begin tri_period_hi <= wdata[2:0]; tri_len <= length_table(wdata); end
          5'h0C: begin noise_vol <= wdata[3:0]; noise_mode <= wdata[7]; end
          5'h0E: begin noise_period_lo <= wdata; end
          5'h0F: begin noise_period_hi <= wdata[2:0]; noise_len <= length_table(wdata); end
          5'h17: begin
            frame_mode <= wdata[7];
            frame_inhibit <= wdata[6];
            frame_div <= 16'd0;
            if (wdata[6]) irq <= 1'b0;
          end
          5'h15: begin
            p1_en <= wdata[0]; p2_en <= wdata[1];
            tri_en <= wdata[2]; noise_en <= wdata[3];
            if (!wdata[2]) tri_len <= 0;
            if (!wdata[3]) noise_len <= 0;
            irq <= 1'b0;
            if (!wdata[0]) p1_len <= 0;
            if (!wdata[1]) p2_len <= 0;
          end
          default: ;
        endcase
      end

      if (cpu_en) begin
        if (frame_div == 16'd29829) begin
          frame_div <= 0;
          if (!frame_inhibit && !frame_mode) irq <= 1'b1;
        end else if (frame_div == 16'd7457 || frame_div == 16'd14914 ||
                     frame_div == 16'd22371) begin
          frame_div <= frame_div + 16'd1;
          frame_step <= frame_step + 2'd1;
          if (!p1_halt && p1_len != 0) p1_len <= p1_len - 8'd1;
          if (!p2_halt && p2_len != 0) p2_len <= p2_len - 8'd1;
          if (tri_len != 0) tri_len <= tri_len - 8'd1;
          if (noise_len != 0) noise_len <= noise_len - 8'd1;
          if (p1_const == 0 && p1_env != 0) p1_env <= p1_env - 8'd1;
          if (p2_const == 0 && p2_env != 0) p2_env <= p2_env - 8'd1;
          if (p1_sweep[7] && p1_shift != 0 && p1_period >= 11'd8) begin
            p1_target = p1_sweep[3] ? p1_period - (p1_period >> p1_shift) :
                                      p1_period + (p1_period >> p1_shift);
            if (p1_target < 11'h800) begin
              p1_period_hi <= p1_target[10:8];
              p1_period_lo <= p1_target[7:0];
            end
          end
          if (p2_sweep[7] && p2_shift != 0 && p2_period >= 11'd8) begin
            p2_target = p2_sweep[3] ? p2_period - (p2_period >> p2_shift) :
                                      p2_period + (p2_period >> p2_shift);
            if (p2_target < 11'h800) begin
              p2_period_hi <= p2_target[10:8];
              p2_period_lo <= p2_target[7:0];
            end
          end
        end else frame_div <= frame_div + 16'd1;

        if (p1_timer == 0) begin
          p1_timer <= {p1_period_hi[2:0], p1_period_lo};
          p1_pos <= p1_pos + 3'd1;
        end else p1_timer <= p1_timer - 8'd1;
        if (p2_timer == 0) begin
          p2_timer <= {p2_period_hi[2:0], p2_period_lo};
          p2_pos <= p2_pos + 3'd1;
        end else p2_timer <= p2_timer - 8'd1;

        if (tri_timer == 0) begin
          tri_timer <= {tri_period_hi[2:0], tri_period_lo};
          tri_pos <= tri_pos + 5'd1;
        end else tri_timer <= tri_timer - 8'd1;

        if (noise_timer == 0) begin
          noise_timer <= {noise_period_hi[2:0], noise_period_lo};
          noise_lfsr <= {noise_lfsr[0] ^ (noise_mode ? noise_lfsr[6] : noise_lfsr[1]),
                         noise_lfsr[14:1]};
        end else noise_timer <= noise_timer - 8'd1;

        sample <= (p1_out + p2_out) * 16'sd512 + tri_out * 16'sd256 + noise_out_w * 16'sd256;
      end
    end
  end
endmodule
