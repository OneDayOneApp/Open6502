// Open6502 CPU core.
//
// Current milestone: reset/fetch, immediate and absolute load/store, basic ALU,
// branches, transfers and flag operations. The remaining official opcodes,
// stack/interrupt/DMA cycles are added in later milestones.
module open6502_cpu (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        cpu_en,
  input  logic [7:0]  bus_rdata,
  input  logic        irq_n,
  input  logic        nmi_n,
  output logic [15:0] bus_addr,
  output logic [7:0]  bus_wdata,
  output logic        bus_we,
  output logic [15:0] dbg_pc,
  output logic [7:0]  dbg_a,
  output logic [7:0]  dbg_x,
  output logic [7:0]  dbg_y,
  output logic [7:0]  dbg_sp,
  output logic [7:0]  dbg_p
);
  typedef enum logic [5:0] {
    ST_RESET_LO, ST_RESET_HI, ST_FETCH, ST_DECODE,
    ST_IMM, ST_ZP, ST_ZPX, ST_ZPY,
    ST_ABS_LO, ST_ABS_HI, ST_ABSX_LO, ST_ABSX_HI,
    ST_ABSY_LO, ST_ABSY_HI, ST_ABS_DATA,
    ST_INDX_PTR, ST_INDX_LO, ST_INDX_HI,
    ST_INDY_PTR, ST_INDY_LO, ST_INDY_HI,
    ST_BRANCH, ST_BRANCH_EXEC, ST_IMPLIED, ST_NOP,
    ST_JSR_PUSH_HI, ST_JSR_PUSH_LO, ST_JSR_SET,
    ST_RTS_PULL_LO, ST_RTS_PULL_HI,
    ST_PUSH_A, ST_PULL_A, ST_PUSH_P, ST_PULL_P, ST_RMW_WRITE,
    ST_BRK_PUSH_HI, ST_BRK_PUSH_LO, ST_BRK_PUSH_P,
    ST_BRK_VEC_LO, ST_BRK_VEC_HI,
    ST_RTI_PULL_P, ST_RTI_PULL_LO, ST_RTI_PULL_HI,
    ST_IRQ_PUSH_HI, ST_IRQ_PUSH_LO, ST_IRQ_PUSH_P,
    ST_IRQ_VEC_LO, ST_IRQ_VEC_HI,
    ST_JMP_IND_LO, ST_JMP_IND_HI, ST_JMP_IND_READ_LO, ST_JMP_IND_READ_HI
  } state_t;

  state_t state;
  logic [15:0] pc;
  logic [15:0] ea;
  logic [7:0]  a, x, y, sp, p;
  logic [7:0]  opcode;
  logic [7:0]  tmp_lo;
  logic [7:0]  zp_ptr;
  logic [7:0]  rmw_data;
  logic        nmi_prev, nmi_pending, irq_nmi;
  logic [8:0]  alu9;
  logic [15:0] ret_addr;
  logic [7:0]  alu8;
  logic        we;

  assign bus_addr  = (state == ST_RESET_LO) ? 16'hFFFC :
                     (state == ST_RESET_HI) ? 16'hFFFD :
                     (state == ST_ZP || state == ST_ZPX || state == ST_ZPY ||
                      state == ST_ABSX_LO || state == ST_ABSX_HI ||
                      state == ST_ABSY_LO || state == ST_ABSY_HI) ? pc :
                     (state == ST_INDX_PTR || state == ST_INDY_PTR) ? pc :
                     (state == ST_INDX_LO || state == ST_INDX_HI ||
                      state == ST_INDY_LO || state == ST_INDY_HI) ? {8'h00, zp_ptr} :
                     (state == ST_JSR_PUSH_HI || state == ST_JSR_PUSH_LO) ?
                       {8'h01, sp} :
                     (state == ST_RTS_PULL_LO) ? {8'h01, sp + 8'd1} :
                     (state == ST_RTS_PULL_HI) ? {8'h01, sp + 8'd1} :
                     (state == ST_PUSH_A || state == ST_PUSH_P ||
                      state == ST_BRK_PUSH_HI || state == ST_BRK_PUSH_LO ||
                      state == ST_BRK_PUSH_P || state == ST_IRQ_PUSH_HI ||
                      state == ST_IRQ_PUSH_LO || state == ST_IRQ_PUSH_P) ?
                       {8'h01, sp} :
                     (state == ST_PULL_A || state == ST_PULL_P ||
                      state == ST_RTI_PULL_P) ? {8'h01, sp + 8'd1} :
                     (state == ST_RTI_PULL_LO) ? {8'h01, sp + 8'd1} :
                     (state == ST_RTI_PULL_HI) ? {8'h01, sp + 8'd1} :
                     (state == ST_BRK_VEC_LO) ? 16'hFFFE :
                     (state == ST_BRK_VEC_HI) ? 16'hFFFF :
                     (state == ST_IRQ_VEC_LO) ? (irq_nmi ? 16'hFFFA : 16'hFFFE) :
                     (state == ST_IRQ_VEC_HI) ? (irq_nmi ? 16'hFFFB : 16'hFFFF) :
                     (state == ST_ABS_DATA || state == ST_RMW_WRITE ||
                      state == ST_JMP_IND_READ_LO) ? ea :
                     (state == ST_JMP_IND_READ_HI) ? {ea[15:8], ea[7:0] + 8'd1} : pc;
  assign ret_addr  = pc - 16'd1;
  assign bus_wdata = (state == ST_JSR_PUSH_HI) ? ret_addr[15:8] :
                     (state == ST_JSR_PUSH_LO) ? ret_addr[7:0] :
                     (state == ST_PUSH_P || state == ST_BRK_PUSH_P) ? (p | 8'h30) :
                     (state == ST_BRK_PUSH_HI) ? pc[15:8] :
                     (state == ST_BRK_PUSH_LO) ? pc[7:0] :
                     (state == ST_IRQ_PUSH_HI) ? pc[15:8] :
                     (state == ST_IRQ_PUSH_LO) ? pc[7:0] :
                     (state == ST_IRQ_PUSH_P) ? ((p & 8'hEF) | 8'h20) :
                     (state == ST_RMW_WRITE) ? rmw_data : a;
  assign bus_we    = we;
  assign dbg_pc    = pc;
  assign dbg_a     = a;
  assign dbg_x     = x;
  assign dbg_y     = y;
  assign dbg_sp    = sp;
  assign dbg_p     = p;

  function automatic [7:0] nz(input [7:0] v, input [7:0] old);
    nz = (old & 8'h7D) | (v == 8'h00 ? 8'h02 : 8'h00) | (v[7] ? 8'h80 : 8'h00);
  endfunction

  always_comb begin
    we = 1'b0;
    if (state == ST_JSR_PUSH_HI || state == ST_JSR_PUSH_LO ||
        state == ST_PUSH_A || state == ST_PUSH_P ||
        state == ST_RMW_WRITE ||
        state == ST_BRK_PUSH_HI || state == ST_BRK_PUSH_LO ||
        state == ST_BRK_PUSH_P ||
        state == ST_IRQ_PUSH_HI || state == ST_IRQ_PUSH_LO ||
        state == ST_IRQ_PUSH_P) begin
      we = 1'b1;
    end else if (state == ST_ABS_DATA) begin
      case (opcode)
        8'h8D, 8'h8E, 8'h8C, 8'h85, 8'h86, 8'h84,
        8'h95, 8'h96, 8'h94, 8'h9D, 8'h99, 8'h81, 8'h91: we = 1'b1;
        default: we = 1'b0;
      endcase
    end
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      state <= ST_RESET_LO;
      pc <= 16'h0000;
      ea <= 16'h0000;
      a <= 8'h00; x <= 8'h00; y <= 8'h00;
      sp <= 8'hFD; p <= 8'h24;
      opcode <= 8'h00; tmp_lo <= 8'h00;
      nmi_prev <= 1'b1; nmi_pending <= 1'b0; irq_nmi <= 1'b0;
    end else if (cpu_en) begin
      nmi_prev <= nmi_n;
      if (!nmi_n && nmi_prev) nmi_pending <= 1'b1;
      case (state)
        ST_RESET_LO: begin
          tmp_lo <= bus_rdata;
          state <= ST_RESET_HI;
        end
        ST_RESET_HI: begin
          pc <= {bus_rdata, tmp_lo};
          state <= ST_FETCH;
        end
        ST_FETCH: begin
          if (nmi_pending) begin
            irq_nmi <= 1'b1;
            state <= ST_IRQ_PUSH_HI;
          end else if (!irq_n && !p[2]) begin
            irq_nmi <= 1'b0;
            state <= ST_IRQ_PUSH_HI;
          end else begin
            opcode <= bus_rdata;
            pc <= pc + 16'd1;
            state <= ST_DECODE;
          end
        end
        ST_DECODE: begin
          case (opcode)
            8'hA9, 8'hA2, 8'hA0, 8'h69, 8'hE9, 8'h29,
            8'h09, 8'h49, 8'hC9, 8'hE0, 8'hC0:
              state <= ST_IMM;
            8'hA5, 8'hA6, 8'hA4, 8'h85, 8'h86, 8'h84,
            8'h65, 8'hE5, 8'h25, 8'h05, 8'h45,
            8'hC5, 8'hE4, 8'hC4, 8'h06, 8'h46, 8'h26, 8'h66,
            8'hE6, 8'hC6, 8'h24:
              state <= ST_ZP;
            8'hB5, 8'h95, 8'h75, 8'hF5, 8'h35, 8'h15, 8'h55,
            8'hD5, 8'hB4, 8'h94, 8'h16, 8'h56, 8'h36, 8'h76,
            8'hF6, 8'hD6:
              state <= ST_ZPX;
            8'hB6, 8'h96:
              state <= ST_ZPY;
            8'hBD, 8'h9D, 8'h7D, 8'hFD, 8'h3D, 8'h1D, 8'h5D,
            8'hDD, 8'hBC, 8'h1E, 8'h5E, 8'h3E, 8'h7E,
            8'hFE, 8'hDE:
              state <= ST_ABSX_LO;
            8'hB9, 8'h99, 8'h79, 8'hF9, 8'h39, 8'h19, 8'h59,
            8'hD9, 8'hBE:
              state <= ST_ABSY_LO;
            8'hA1, 8'h81, 8'h61, 8'hE1, 8'h21, 8'h01, 8'h41, 8'hC1:
              state <= ST_INDX_PTR;
            8'hB1, 8'h91, 8'h71, 8'hF1, 8'h31, 8'h11, 8'h51, 8'hD1:
              state <= ST_INDY_PTR;
            8'hAD, 8'hAE, 8'hAC, 8'h8D, 8'h8E, 8'h8C, 8'h4C, 8'h20,
            8'h0E, 8'h4E, 8'h2E, 8'h6E, 8'hEE, 8'hCE,
            8'h2C, 8'hEC, 8'hCC:
              state <= ST_ABS_LO;
            8'h6C:
              state <= ST_JMP_IND_LO;
            8'h60:
              state <= ST_RTS_PULL_LO;
            8'h48:
              state <= ST_PUSH_A;
            8'h68:
              state <= ST_PULL_A;
            8'h08:
              state <= ST_PUSH_P;
            8'h28:
              state <= ST_PULL_P;
            8'h00:
              state <= ST_BRK_PUSH_HI;
            8'h40:
              state <= ST_RTI_PULL_P;
            8'hD0, 8'hF0, 8'h90, 8'hB0, 8'h10, 8'h30,
            8'h50, 8'h70:
              state <= ST_BRANCH;
            default:
              state <= ST_IMPLIED;
          endcase
        end
        ST_IMM: begin
          case (opcode)
            8'hA9: begin a <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hA2: begin x <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hA0: begin y <= bus_rdata; p <= nz(bus_rdata, p); end
            8'h69: begin
              alu9 = {1'b0, a} + {1'b0, bus_rdata} + {8'd0, p[0]};
              a <= alu9[7:0];
              p <= (nz(alu9[7:0], p) & 8'hBF) | (alu9[8] ? 8'h01 : 8'h00) |
                   ((~(a[7] ^ bus_rdata[7]) & (a[7] ^ alu9[7])) ? 8'h40 : 8'h00);
            end
            8'hE9: begin
              alu9 = {1'b0, a} + {1'b0, ~bus_rdata} + {8'd0, p[0]};
              a <= alu9[7:0];
              p <= (nz(alu9[7:0], p) & 8'hBF) | (alu9[8] ? 8'h01 : 8'h00) |
                   (((a[7] ^ bus_rdata[7]) & (a[7] ^ alu9[7])) ? 8'h40 : 8'h00);
            end
            8'h29: begin a <= a & bus_rdata; p <= nz(a & bus_rdata, p); end
            8'h09: begin a <= a | bus_rdata; p <= nz(a | bus_rdata, p); end
            8'h49: begin a <= a ^ bus_rdata; p <= nz(a ^ bus_rdata, p); end
            8'hC9: begin
              alu9 = {1'b0, a} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (a >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'hE0: begin
              alu9 = {1'b0, x} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (x >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'hC0: begin
              alu9 = {1'b0, y} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (y >= bus_rdata ? 8'h01 : 8'h00);
            end
            default: ;
          endcase
          pc <= pc + 16'd1;
          state <= ST_FETCH;
        end
        ST_ZP: begin
          ea <= {8'h00, bus_rdata};
          pc <= pc + 16'd1;
          state <= ST_ABS_DATA;
        end
        ST_ZPX: begin
          ea <= {8'h00, bus_rdata + x};
          pc <= pc + 16'd1;
          state <= ST_ABS_DATA;
        end
        ST_ZPY: begin
          ea <= {8'h00, bus_rdata + y};
          pc <= pc + 16'd1;
          state <= ST_ABS_DATA;
        end
        ST_ABSX_LO: begin
          tmp_lo <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_ABSX_HI;
        end
        ST_ABSX_HI: begin
          ea <= {bus_rdata, tmp_lo} + {8'h00, x};
          pc <= pc + 16'd1;
          state <= ST_ABS_DATA;
        end
        ST_ABSY_LO: begin
          tmp_lo <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_ABSY_HI;
        end
        ST_ABSY_HI: begin
          ea <= {bus_rdata, tmp_lo} + {8'h00, y};
          pc <= pc + 16'd1;
          state <= ST_ABS_DATA;
        end
        ST_INDX_PTR: begin
          zp_ptr <= bus_rdata + x;
          pc <= pc + 16'd1;
          state <= ST_INDX_LO;
        end
        ST_INDX_LO: begin
          tmp_lo <= bus_rdata;
          zp_ptr <= zp_ptr + 8'd1;
          state <= ST_INDX_HI;
        end
        ST_INDX_HI: begin
          ea <= {bus_rdata, tmp_lo};
          state <= ST_ABS_DATA;
        end
        ST_INDY_PTR: begin
          zp_ptr <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_INDY_LO;
        end
        ST_INDY_LO: begin
          tmp_lo <= bus_rdata;
          zp_ptr <= zp_ptr + 8'd1;
          state <= ST_INDY_HI;
        end
        ST_INDY_HI: begin
          ea <= {bus_rdata, tmp_lo} + {8'h00, y};
          state <= ST_ABS_DATA;
        end
        ST_JMP_IND_LO: begin
          tmp_lo <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_JMP_IND_HI;
        end
        ST_JMP_IND_HI: begin
          ea <= {bus_rdata, tmp_lo};
          pc <= pc + 16'd1;
          state <= ST_JMP_IND_READ_LO;
        end
        ST_JMP_IND_READ_LO: begin
          tmp_lo <= bus_rdata;
          state <= ST_JMP_IND_READ_HI;
        end
        ST_JMP_IND_READ_HI: begin
          pc <= {bus_rdata, tmp_lo};
          state <= ST_FETCH;
        end
        ST_ABS_LO: begin
          tmp_lo <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_ABS_HI;
        end
        ST_ABS_HI: begin
          ea <= {bus_rdata, tmp_lo};
          pc <= pc + 16'd1;
          if (opcode == 8'h4C) begin
            pc <= {bus_rdata, tmp_lo};
            state <= ST_FETCH;
          end else if (opcode == 8'h20) begin
            state <= ST_JSR_PUSH_HI;
          end else begin
            state <= ST_ABS_DATA;
          end
        end
        ST_ABS_DATA: begin
          state <= ST_FETCH;
          case (opcode)
            8'hAD, 8'hA5, 8'hB5, 8'hBD, 8'hB9, 8'hA1, 8'hB1:
              begin a <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hAE, 8'hA6, 8'hB6, 8'hBE:
              begin x <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hAC, 8'hA4, 8'hB4, 8'hBC:
              begin y <= bus_rdata; p <= nz(bus_rdata, p); end
            8'h8D, 8'h85, 8'h95, 8'h9D, 8'h99, 8'h81, 8'h91,
            8'h8E, 8'h86, 8'h96, 8'h8C, 8'h84, 8'h94: ;
            8'h65, 8'h75, 8'h7D, 8'h79, 8'h61, 8'h71: begin
              alu9 = {1'b0, a} + {1'b0, bus_rdata} + {8'd0, p[0]};
              a <= alu9[7:0];
              p <= (nz(alu9[7:0], p) & 8'hBF) | (alu9[8] ? 8'h01 : 8'h00) |
                   ((~(a[7] ^ bus_rdata[7]) & (a[7] ^ alu9[7])) ? 8'h40 : 8'h00);
            end
            8'hE5, 8'hF5, 8'hFD, 8'hF9, 8'hE1, 8'hF1: begin
              alu9 = {1'b0, a} + {1'b0, ~bus_rdata} + {8'd0, p[0]};
              a <= alu9[7:0];
              p <= (nz(alu9[7:0], p) & 8'hBF) | (alu9[8] ? 8'h01 : 8'h00) |
                   (((a[7] ^ bus_rdata[7]) & (a[7] ^ alu9[7])) ? 8'h40 : 8'h00);
            end
            8'h25, 8'h35, 8'h3D, 8'h39, 8'h21, 8'h31: begin a <= a & bus_rdata; p <= nz(a & bus_rdata, p); end
            8'h05, 8'h15, 8'h1D, 8'h19, 8'h01, 8'h11: begin a <= a | bus_rdata; p <= nz(a | bus_rdata, p); end
            8'h45, 8'h55, 8'h5D, 8'h59, 8'h41, 8'h51: begin a <= a ^ bus_rdata; p <= nz(a ^ bus_rdata, p); end
            8'hC5, 8'hD5, 8'hDD, 8'hD9, 8'hC1, 8'hD1: begin
              alu9 = {1'b0, a} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (a >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'hE6, 8'hF6, 8'hEE, 8'hFE: begin
              rmw_data <= bus_rdata + 8'd1;
              p <= nz(bus_rdata + 8'd1, p);
              state <= ST_RMW_WRITE;
            end
            8'hC6, 8'hD6, 8'hCE, 8'hDE: begin
              rmw_data <= bus_rdata - 8'd1;
              p <= nz(bus_rdata - 8'd1, p);
              state <= ST_RMW_WRITE;
            end
            8'h24, 8'h2C: begin
              p[7] <= bus_rdata[7];
              p[6] <= bus_rdata[6];
              p[1] <= ((a & bus_rdata) == 8'h00);
            end
            8'hEC: begin
              alu9 = {1'b0, x} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (x >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'hCC: begin
              alu9 = {1'b0, y} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (y >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'h06, 8'h16, 8'h0E, 8'h1E: begin
              rmw_data <= {bus_rdata[6:0], 1'b0};
              p <= (nz({bus_rdata[6:0],1'b0}, p) & 8'hFE) | (bus_rdata[7] ? 8'h01 : 8'h00);
              state <= ST_RMW_WRITE;
            end
            8'h46, 8'h56, 8'h4E, 8'h5E: begin
              rmw_data <= {1'b0, bus_rdata[7:1]};
              p <= (nz({1'b0,bus_rdata[7:1]}, p) & 8'hFE) | (bus_rdata[0] ? 8'h01 : 8'h00);
              state <= ST_RMW_WRITE;
            end
            8'h26, 8'h36, 8'h2E, 8'h3E: begin
              rmw_data <= {bus_rdata[6:0], p[0]};
              p <= (nz({bus_rdata[6:0],p[0]}, p) & 8'hFE) | (bus_rdata[7] ? 8'h01 : 8'h00);
              state <= ST_RMW_WRITE;
            end
            8'h66, 8'h76, 8'h6E, 8'h7E: begin
              rmw_data <= {p[0], bus_rdata[7:1]};
              p <= (nz({p[0],bus_rdata[7:1]}, p) & 8'hFE) | (bus_rdata[0] ? 8'h01 : 8'h00);
              state <= ST_RMW_WRITE;
            end
            8'hE4: begin
              alu9 = {1'b0, x} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (x >= bus_rdata ? 8'h01 : 8'h00);
            end
            8'hC4: begin
              alu9 = {1'b0, y} + {1'b0, ~bus_rdata} + 9'd1;
              p <= (nz(alu9[7:0], p) & 8'hFE) | (y >= bus_rdata ? 8'h01 : 8'h00);
            end
            default: ;
          endcase
        end
        ST_IRQ_PUSH_HI: begin
          sp <= sp - 8'd1;
          state <= ST_IRQ_PUSH_LO;
        end
        ST_IRQ_PUSH_LO: begin
          sp <= sp - 8'd1;
          state <= ST_IRQ_PUSH_P;
        end
        ST_IRQ_PUSH_P: begin
          sp <= sp - 8'd1;
          p <= p | 8'h04;
          state <= ST_IRQ_VEC_LO;
        end
        ST_IRQ_VEC_LO: begin
          tmp_lo <= bus_rdata;
          state <= ST_IRQ_VEC_HI;
        end
        ST_IRQ_VEC_HI: begin
          pc <= {bus_rdata, tmp_lo};
          nmi_pending <= 1'b0;
          state <= ST_FETCH;
        end
        ST_BRK_PUSH_HI: begin
          sp <= sp - 8'd1;
          state <= ST_BRK_PUSH_LO;
        end
        ST_BRK_PUSH_LO: begin
          sp <= sp - 8'd1;
          state <= ST_BRK_PUSH_P;
        end
        ST_BRK_PUSH_P: begin
          sp <= sp - 8'd1;
          p <= p | 8'h04; // I
          state <= ST_BRK_VEC_LO;
        end
        ST_BRK_VEC_LO: begin
          tmp_lo <= bus_rdata;
          state <= ST_BRK_VEC_HI;
        end
        ST_BRK_VEC_HI: begin
          pc <= {bus_rdata, tmp_lo};
          state <= ST_FETCH;
        end
        ST_RTI_PULL_P: begin
          sp <= sp + 8'd1;
          p <= (bus_rdata & 8'hCF) | 8'h20;
          state <= ST_RTI_PULL_LO;
        end
        ST_RTI_PULL_LO: begin
          sp <= sp + 8'd1;
          tmp_lo <= bus_rdata;
          state <= ST_RTI_PULL_HI;
        end
        ST_RTI_PULL_HI: begin
          sp <= sp + 8'd1;
          pc <= {bus_rdata, tmp_lo};
          state <= ST_FETCH;
        end
        ST_JSR_PUSH_HI: begin
          sp <= sp - 8'd1;
          state <= ST_JSR_PUSH_LO;
        end
        ST_JSR_PUSH_LO: begin
          sp <= sp - 8'd1;
          state <= ST_JSR_SET;
        end
        ST_JSR_SET: begin
          pc <= ea;
          state <= ST_FETCH;
        end
        ST_RTS_PULL_LO: begin
          sp <= sp + 8'd1;
          tmp_lo <= bus_rdata;
          state <= ST_RTS_PULL_HI;
        end
        ST_RTS_PULL_HI: begin
          sp <= sp + 8'd1;
          pc <= {bus_rdata, tmp_lo} + 16'd1;
          state <= ST_FETCH;
        end
        ST_PUSH_A: begin
          sp <= sp - 8'd1;
          state <= ST_FETCH;
        end
        ST_PULL_A: begin
          sp <= sp + 8'd1;
          a <= bus_rdata;
          p <= nz(bus_rdata, p);
          state <= ST_FETCH;
        end
        ST_PUSH_P: begin
          sp <= sp - 8'd1;
          state <= ST_FETCH;
        end
        ST_PULL_P: begin
          sp <= sp + 8'd1;
          p <= (bus_rdata & 8'hCF) | 8'h20;
          state <= ST_FETCH;
        end
        ST_RMW_WRITE: state <= ST_FETCH;
        ST_BRANCH: begin
          state <= ST_BRANCH_EXEC;
        end
        ST_BRANCH_EXEC: begin
          pc <= pc + 16'd1;
          if ((opcode == 8'hD0 && !p[1]) || (opcode == 8'hF0 && p[1]) ||
              (opcode == 8'h90 && !p[0]) || (opcode == 8'hB0 && p[0]) ||
              (opcode == 8'h10 && !p[7]) || (opcode == 8'h30 && p[7]) ||
              (opcode == 8'h50 && !p[6]) || (opcode == 8'h70 && p[6])) begin
            pc <= pc + 16'd1 + {{8{bus_rdata[7]}}, bus_rdata};
          end
          state <= ST_FETCH;
        end
        ST_IMPLIED: begin
          case (opcode)
            8'hAA: begin x <= a; p <= nz(a, p); end
            8'h8A: begin a <= x; p <= nz(x, p); end
            8'hA8: begin y <= a; p <= nz(a, p); end
            8'h98: begin a <= y; p <= nz(y, p); end
            8'hBA: begin x <= sp; p <= nz(sp, p); end
            8'h9A: sp <= x;
            8'hE8: begin x <= x + 8'd1; p <= nz(x + 8'd1, p); end
            8'hC8: begin y <= y + 8'd1; p <= nz(y + 8'd1, p); end
            8'hCA: begin x <= x - 8'd1; p <= nz(x - 8'd1, p); end
            8'h88: begin y <= y - 8'd1; p <= nz(y - 8'd1, p); end
            8'h0A: begin p[0] <= a[7]; a <= {a[6:0], 1'b0}; p <= nz({a[6:0],1'b0}, p); end
            8'h4A: begin p[0] <= a[0]; a <= {1'b0, a[7:1]}; p <= nz({1'b0,a[7:1]}, p); end
            8'h2A: begin p[0] <= a[7]; a <= {a[6:0], p[0]}; p <= nz({a[6:0],p[0]}, p); end
            8'h6A: begin p[0] <= a[0]; a <= {p[0], a[7:1]}; p <= nz({p[0],a[7:1]}, p); end
            8'h18: p[0] <= 1'b0;
            8'h38: p[0] <= 1'b1;
            8'h58: p[2] <= 1'b0;
            8'h78: p[2] <= 1'b1;
            8'hB8: p[6] <= 1'b0;
            8'hD8: p[3] <= 1'b0;
            8'hF8: p[3] <= 1'b1;
            default: ;
          endcase
          state <= ST_FETCH;
        end
        ST_NOP: state <= ST_FETCH;
        default: state <= ST_FETCH;
      endcase
    end
  end
endmodule
