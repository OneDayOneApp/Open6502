// Open6502 cartridge mapper front end.
//
// v0 supports NROM (0), UxROM (2), CNROM (3) and AxROM (7) bank registers.
// The address outputs are 15-bit for the current 32 KiB cartridge prototype.
module open6502_mapper (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [2:0]  mapper,
  input  logic [15:0] cpu_addr,
  input  logic        cpu_we,
  input  logic [7:0]  cpu_wdata,
  input  logic        ppu_a12,
  output logic [14:0] prg_addr,
  output logic [12:0] chr_addr,
  output logic        mirroring,
  output logic        irq
);
  logic [2:0] prg_bank;
  logic [1:0] chr_bank;
  logic       mirror;
  logic [4:0] mmc1_shift;
  logic [2:0] mmc1_count;
  logic [3:0] mmc1_ctrl, mmc1_chr0, mmc1_chr1, mmc1_prg;
  logic [7:0] mmc3_regs [0:7];
  logic [2:0] mmc3_sel;
  logic       mmc3_prg_mode, mmc3_chr_mode;
  logic [7:0] mmc3_latch, mmc3_counter;
  logic       mmc3_reload, mmc3_enable, mmc3_irq;
  logic       a12_prev;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      prg_bank <= 3'd0;
      chr_bank <= 2'd0;
      mirror <= 1'b0;
      mmc1_shift <= 5'd0; mmc1_count <= 3'd0;
      mmc1_ctrl <= 4'h0; mmc1_chr0 <= 4'h0;
      mmc1_chr1 <= 4'h0; mmc1_prg <= 4'h0;
      for (int i = 0; i < 8; i++) mmc3_regs[i] <= 8'h00;
      mmc3_sel <= 3'd0; mmc3_prg_mode <= 1'b0; mmc3_chr_mode <= 1'b0;
      mmc3_latch <= 8'h00; mmc3_counter <= 8'h00;
      mmc3_reload <= 1'b0; mmc3_enable <= 1'b0; mmc3_irq <= 1'b0;
      a12_prev <= 1'b0;
    end else if (cpu_we && cpu_addr >= 16'h8000) begin
      case (mapper)
        3'd4: begin
          case (cpu_addr[14:13])
            2'b00: begin
              if (cpu_addr[0] == 1'b0) begin
                mmc3_sel <= cpu_wdata[2:0];
                mmc3_prg_mode <= cpu_wdata[6];
                mmc3_chr_mode <= cpu_wdata[7];
              end else mmc3_regs[mmc3_sel] <= cpu_wdata;
            end
            2'b01: mirror <= cpu_wdata[0];
            2'b10: begin
              if (cpu_addr[0] == 1'b0) mmc3_latch <= cpu_wdata;
              else mmc3_reload <= 1'b1;
            end
            2'b11: begin
              if (cpu_addr[0] == 1'b0) begin
                mmc3_enable <= 1'b0;
                mmc3_irq <= 1'b0;
              end else mmc3_enable <= 1'b1;
            end
          endcase
        end
        3'd1: begin
          if (cpu_wdata[7]) begin
            mmc1_shift <= 5'd0;
            mmc1_count <= 3'd0;
            mmc1_ctrl <= mmc1_ctrl | 4'hC;
          end else begin
            mmc1_shift <= {cpu_wdata[0], mmc1_shift[4:1]};
            if (mmc1_count == 3'd4) begin
              case (cpu_addr[14:13])
                2'd0: mmc1_ctrl <= mmc1_shift;
                2'd1: mmc1_chr0 <= mmc1_shift[3:0];
                2'd2: mmc1_chr1 <= mmc1_shift[3:0];
                default: mmc1_prg <= mmc1_shift[3:0];
              endcase
              mmc1_count <= 3'd0;
            end else mmc1_count <= mmc1_count + 3'd1;
          end
        end
        3'd2: prg_bank <= cpu_wdata[2:0];
        3'd3: chr_bank <= cpu_wdata[1:0];
        3'd7: begin
          prg_bank <= cpu_wdata[2:0];
          mirror <= cpu_wdata[4];
        end
        default: ;
      endcase
    end

    if (mapper == 3'd4) begin
      a12_prev <= ppu_a12;
      if (ppu_a12 && !a12_prev) begin
        if (mmc3_counter == 8'd0 || mmc3_reload) begin
          mmc3_counter <= mmc3_latch;
          mmc3_reload <= 1'b0;
        end else mmc3_counter <= mmc3_counter - 8'd1;
        if (mmc3_counter == 8'd0 && mmc3_enable) mmc3_irq <= 1'b1;
      end
    end
  end

  assign irq = mmc3_irq;

  always_comb begin
    prg_addr = cpu_addr[14:0];
    chr_addr = cpu_addr[12:0];
    mirroring = mirror;
    case (mapper)
      3'd1: begin
        mirroring = mmc1_ctrl[1:0] == 2'b10;
        case (mmc1_ctrl[3:2])
          2'd0, 2'd1: prg_addr = {mmc1_prg[3:1], 13'd0} + cpu_addr[14:0];
          2'd2: prg_addr = (cpu_addr < 16'hC000) ? cpu_addr[14:0] :
                             {mmc1_prg[3:0], 12'd0} + cpu_addr[13:0];
          default: prg_addr = (cpu_addr < 16'hC000) ?
                               {mmc1_prg[3:0], 12'd0} + cpu_addr[13:0] :
                               15'h4000 + cpu_addr[13:0];
        endcase
        if (cpu_addr < 16'h2000) begin
          if (cpu_addr < 16'h1000)
            chr_addr = {mmc1_chr0, 12'd0} + cpu_addr[11:0];
          else
            chr_addr = {mmc1_chr1, 12'd0} + cpu_addr[11:0];
        end else chr_addr = cpu_addr[12:0];
      end
      3'd2: begin
        if (cpu_addr < 16'hC000)
          prg_addr = {prg_bank, 12'd0} + cpu_addr[13:0];
        else
          prg_addr = 15'h4000 + cpu_addr[13:0];
      end
      3'd3: begin
        prg_addr = cpu_addr[14:0];
        chr_addr = {chr_bank, 11'd0} + cpu_addr[12:0];
      end
      3'd7: begin
        prg_addr = {prg_bank, 12'd0} + cpu_addr[14:0];
        mirroring = mirror;
      end
      3'd4: begin
        prg_addr = cpu_addr[14:0];
        chr_addr = cpu_addr[12:0];
        if (cpu_addr < 16'h2000) begin
          case (cpu_addr[12:10])
            3'd0: chr_addr = {mmc3_regs[0][2:0], 10'd0} + cpu_addr[9:0];
            3'd1: chr_addr = {mmc3_regs[0][2:0], 10'd0} + 13'h0400 + cpu_addr[9:0];
            3'd2: chr_addr = {mmc3_regs[1][2:0], 10'd0} + cpu_addr[9:0];
            3'd3: chr_addr = {mmc3_regs[1][2:0], 10'd0} + 13'h0400 + cpu_addr[9:0];
            3'd4: chr_addr = {mmc3_regs[2][2:0], 10'd0} + cpu_addr[9:0];
            3'd5: chr_addr = {mmc3_regs[3][2:0], 10'd0} + cpu_addr[9:0];
            3'd6: chr_addr = {mmc3_regs[4][2:0], 10'd0} + cpu_addr[9:0];
            default: chr_addr = {mmc3_regs[5][2:0], 10'd0} + cpu_addr[9:0];
          endcase
        end
        case (cpu_addr[14:13])
          2'b00: prg_addr = mmc3_prg_mode ? 15'h4000 + cpu_addr[12:0] :
                             {mmc3_regs[6][1:0], 13'd0} + cpu_addr[12:0];
          2'b01: prg_addr = {mmc3_regs[7][1:0], 13'd0} + cpu_addr[12:0];
          2'b10: prg_addr = mmc3_prg_mode ? {mmc3_regs[6][1:0], 13'd0} + cpu_addr[12:0] :
                             15'h4000 + cpu_addr[12:0];
          default: prg_addr = 15'h4000 + cpu_addr[12:0];
        endcase
      end
      default: begin
        prg_addr = cpu_addr[14:0];
        chr_addr = cpu_addr[12:0];
      end
    endcase
  end
endmodule
