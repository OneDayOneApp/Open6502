// Open6502 PPU register/timing block.
//
// This milestone provides the CPU-visible PPU registers, 2 KB VRAM, 256-byte
// OAM, palette RAM, vblank/NMI and $2007 auto-increment. Background/sprite
// pixel generation is added in later milestones.
module open6502_ppu (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        ppu_en,
  input  logic [8:0]  x,
  input  logic [8:0]  y,
  input  logic        cs,
  input  logic        we,
  input  logic [2:0]  addr,
  input  logic [7:0]  wdata,
  output logic [7:0]  rdata,
  output logic        nmi
);
  logic [7:0]  ctrl, mask, status, oam_addr;
  logic [7:0]  vram [0:2047];
  logic [7:0]  oam  [0:255];
  logic [7:0]  pal  [0:31];
  logic [14:0] vaddr;
  logic [7:0]  read_buf;
  logic        w_toggle;
  logic        nmi_level;

  assign nmi = nmi_level;

  always_comb begin
    case (addr)
      3'd2: rdata = status;
      3'd4: rdata = oam[oam_addr];
      3'd7: rdata = read_buf;
      default: rdata = 8'h00;
    endcase
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      ctrl <= 8'h00; mask <= 8'h00; status <= 8'h00; oam_addr <= 8'h00;
      vaddr <= 15'h0000; read_buf <= 8'h00; w_toggle <= 1'b0;
      nmi_level <= 1'b0;
      for (int i = 0; i < 2048; i++) vram[i] <= 8'h00;
      for (int i = 0; i < 256; i++) oam[i] <= 8'h00;
      for (int i = 0; i < 32; i++) pal[i] <= 8'h00;
    end else begin
      if (ppu_en && y == 9'd241 && x == 9'd1) begin
        status[7] <= 1'b1;
        if (ctrl[7]) nmi_level <= 1'b1;
      end
      if (ppu_en && y == 9'd261 && x == 9'd1) begin
        status[7] <= 1'b0;
        nmi_level <= 1'b0;
      end

      if (cs) begin
        if (we) begin
          case (addr)
            3'd0: ctrl <= wdata;
            3'd1: mask <= wdata;
            3'd3: oam_addr <= wdata;
            3'd4: begin
              oam[oam_addr] <= wdata;
              oam_addr <= oam_addr + 8'd1;
            end
            3'd5: w_toggle <= ~w_toggle;
            3'd6: begin
              if (!w_toggle) vaddr[14:8] <= wdata[6:0];
              else           vaddr[7:0]  <= wdata;
              w_toggle <= ~w_toggle;
            end
            3'd7: begin
              if (vaddr[13:8] == 6'h3F) pal[vaddr[4:0]] <= wdata;
              else vram[vaddr[10:0]] <= wdata;
              vaddr <= vaddr + (ctrl[2] ? 15'd32 : 15'd1);
            end
            default: ;
          endcase
        end else begin
          case (addr)
            3'd2: begin
              status[7] <= 1'b0;
              nmi_level <= 1'b0;
              w_toggle <= 1'b0;
            end
            3'd4: ;
            3'd7: begin
              if (vaddr[13:8] == 6'h3F) read_buf <= pal[vaddr[4:0]];
              else read_buf <= vram[vaddr[10:0]];
              vaddr <= vaddr + (ctrl[2] ? 15'd32 : 15'd1);
            end
            default: ;
          endcase
        end
      end
    end
  end
endmodule
