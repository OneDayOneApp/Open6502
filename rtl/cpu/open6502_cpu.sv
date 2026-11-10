// Open6502 CPU front end.
//
// This is the first RTL milestone: reset-vector fetch, instruction fetch and a
// small directed subset (LDA #, STA abs, JMP abs, BNE, NOP). The remaining
// official opcodes/addressing modes are added in later milestones.
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
    ST_IMM, ST_ABS_LO, ST_ABS_HI, ST_STA, ST_JMP,
    ST_BRANCH, ST_BRANCH_EXEC, ST_NOP
  } state_t;

  state_t state;
  logic [15:0] pc;
  logic [7:0]  a, x, y, sp, p;
  logic [7:0]  opcode;
  logic [7:0]  tmp_lo, tmp_hi;
  logic [15:0] addr;
  logic        we;

  assign bus_addr  = addr;
  assign bus_wdata = a;
  assign bus_we    = we;
  assign dbg_pc    = pc;
  assign dbg_a     = a;
  assign dbg_x     = x;
  assign dbg_y     = y;
  assign dbg_sp    = sp;
  assign dbg_p     = p;

  always_comb begin
    addr = pc;
    we   = 1'b0;
    case (state)
      ST_RESET_LO: addr = 16'hFFFC;
      ST_RESET_HI: addr = 16'hFFFD;
      ST_ABS_HI:   addr = pc;
      ST_STA: begin
        addr = {tmp_hi, tmp_lo};
        we   = 1'b1;
      end
      ST_BRANCH:      addr = pc;
      ST_BRANCH_EXEC: addr = pc;
      default:     addr = pc;
    endcase
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      state <= ST_RESET_LO;
      pc <= 16'h0000;
      a <= 8'h00; x <= 8'h00; y <= 8'h00;
      sp <= 8'hFD; p <= 8'h24;
      opcode <= 8'h00; tmp_lo <= 8'h00; tmp_hi <= 8'h00;
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
            8'hA9: state <= ST_IMM;      // LDA #imm
            8'h8D: state <= ST_ABS_LO;   // STA abs
            8'h4C: state <= ST_ABS_LO;   // JMP abs
            8'hD0: state <= ST_BRANCH;   // BNE rel
            8'hEA: state <= ST_NOP;      // NOP
            default: state <= ST_NOP;
          endcase
        end
        ST_IMM: begin
          a <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_FETCH;
        end
        ST_ABS_LO: begin
          tmp_lo <= bus_rdata;
          pc <= pc + 16'd1;
          state <= ST_ABS_HI;
        end
        ST_ABS_HI: begin
          tmp_hi <= bus_rdata;
          pc <= pc + 16'd1;
          if (opcode == 8'h4C) state <= ST_JMP;
          else                 state <= ST_STA;
        end
        ST_STA: begin
          state <= ST_FETCH;
        end
        ST_JMP: begin
          pc <= {tmp_hi, tmp_lo};
          state <= ST_FETCH;
        end
        ST_BRANCH: state <= ST_BRANCH_EXEC;
        ST_BRANCH_EXEC: begin
          pc <= pc + 16'd1;
          if (!p[1]) pc <= pc + {{8{bus_rdata[7]}}, bus_rdata};
          state <= ST_FETCH;
        end
        ST_NOP: state <= ST_FETCH;
        default: state <= ST_FETCH;
      endcase
    end
  end
endmodule
