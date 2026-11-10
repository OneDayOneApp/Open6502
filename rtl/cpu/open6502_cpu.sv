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
  typedef enum logic [3:0] {
    ST_RESET_LO, ST_RESET_HI, ST_FETCH, ST_DECODE,
    ST_IMM, ST_ABS_LO, ST_ABS_HI, ST_ABS_DATA,
    ST_BRANCH, ST_BRANCH_EXEC, ST_IMPLIED, ST_NOP
  } state_t;

  state_t state;
  logic [15:0] pc;
  logic [15:0] ea;
  logic [7:0]  a, x, y, sp, p;
  logic [7:0]  opcode;
  logic [7:0]  tmp_lo;
  logic [8:0]  alu9;
  logic [7:0]  alu8;
  logic        we;

  assign bus_addr  = (state == ST_RESET_LO) ? 16'hFFFC :
                     (state == ST_RESET_HI) ? 16'hFFFD :
                     (state == ST_ABS_DATA) ? ea : pc;
  assign bus_wdata = a;
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
    if (state == ST_ABS_DATA) begin
      case (opcode)
        8'h8D, 8'h8E, 8'h8C: we = 1'b1; // STA/STX/STY abs
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
    end else if (cpu_en) begin
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
          opcode <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_DECODE;
        end
        ST_DECODE: begin
          case (opcode)
            8'hA9, 8'hA2, 8'hA0, 8'h69, 8'hE9, 8'h29,
            8'h09, 8'h49, 8'hC9, 8'hE0, 8'hC0:
              state <= ST_IMM;
            8'hAD, 8'hAE, 8'hAC, 8'h8D, 8'h8E, 8'h8C, 8'h4C:
              state <= ST_ABS_LO;
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
          end else begin
            state <= ST_ABS_DATA;
          end
        end
        ST_ABS_DATA: begin
          case (opcode)
            8'hAD: begin a <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hAE: begin x <= bus_rdata; p <= nz(bus_rdata, p); end
            8'hAC: begin y <= bus_rdata; p <= nz(bus_rdata, p); end
            8'h8D, 8'h8E, 8'h8C: ; // write performed by bus_we
            default: ;
          endcase
          state <= ST_FETCH;
        end
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
