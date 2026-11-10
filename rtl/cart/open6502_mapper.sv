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
  output logic [14:0] prg_addr,
  output logic [12:0] chr_addr,
  output logic        mirroring
);
  logic [2:0] prg_bank;
  logic [1:0] chr_bank;
  logic       mirror;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      prg_bank <= 3'd0;
      chr_bank <= 2'd0;
      mirror <= 1'b0;
    end else if (cpu_we && cpu_addr >= 16'h8000) begin
      case (mapper)
        3'd2: prg_bank <= cpu_wdata[2:0];
        3'd3: chr_bank <= cpu_wdata[1:0];
        3'd7: begin
          prg_bank <= cpu_wdata[2:0];
          mirror <= cpu_wdata[4];
        end
        default: ;
      endcase
    end
  end

  always_comb begin
    prg_addr = cpu_addr[14:0];
    chr_addr = cpu_addr[12:0];
    mirroring = mirror;
    case (mapper)
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
      default: begin
        prg_addr = cpu_addr[14:0];
        chr_addr = cpu_addr[12:0];
      end
    endcase
  end
endmodule
