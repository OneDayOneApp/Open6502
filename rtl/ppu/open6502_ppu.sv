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
  input  logic        dma_we,
  input  logic [7:0]  dma_addr,
  input  logic [7:0]  dma_wdata,
  output logic [7:0]  rdata,
  output logic [7:0]  pixel_index,
  output logic        sprite0_hit,
  output logic        nmi
);
  logic [7:0]  ctrl, mask, status, oam_addr;
  logic [7:0]  chr  [0:8191];
  logic [7:0]  vram [0:2047];
  logic [7:0]  oam  [0:255];
  logic [7:0]  pal  [0:31];
  logic [14:0] vaddr;
  logic [7:0]  scroll_x, scroll_y;
  logic [7:0]  read_buf;
  logic        w_toggle;
  logic        nmi_level;
  logic [7:0]  tile, attr, lo, hi;
  logic [12:0] pat;
  logic [1:0]  color, pal_bits;
  logic        bg_opaque;
  logic [7:0]  sprite_data;
  logic [7:0]  sy, sx, stile, sattr, srow, scol;
  logic [12:0] spat;
  logic [1:0]  scolor;
  integer      tx, ty, fine_y, bitpos, i, sbit;
  integer      px, py, ntx, nty, nt;

  assign nmi = nmi_level;

  always_comb begin
    pixel_index = pal[0];
    sprite0_hit = 1'b0;
    bg_opaque = 1'b0;
    if ((mask[3]) && (x < 9'd256) && (y < 9'd240)) begin
      px = x + scroll_x;
      py = y + scroll_y;
      tx = (px >> 3) & 31;
      ty = (py >> 3) % 30;
      ntx = (px >> 8) & 1;
      nty = (py >> 8) & 1;
      nt = ntx ^ nty;
      fine_y = py[2:0];
      tile = vram[nt * 11'h400 + ty * 32 + tx];
      attr = vram[nt * 11'h400 + 11'h3C0 + (ty >> 2) * 8 + (tx >> 2)];
      pal_bits = (attr >> (((ty & 2) << 1) | (tx & 2))) & 2'b11;
      pat = (ctrl[4] ? 13'h1000 : 13'h0000) + {4'd0, tile, 4'd0} + fine_y[3:0];
      lo = chr[pat[12:0]];
      hi = chr[pat[12:0] + 13'd8];
      bitpos = 7 - px[2:0];
      color = {hi[bitpos], lo[bitpos]};
      if (color != 2'b00) begin
        pixel_index = pal[{pal_bits, color}];
        bg_opaque = 1'b1;
      end else begin
        pixel_index = pal[0];
      end
    end

    if ((mask[4]) && (x < 9'd256) && (y < 9'd240)) begin
      for (i = 0; i < 64; i = i + 1) begin
        sy = oam[i*4] + 8'd1;
        sx = oam[i*4 + 3];
        if (y >= sy && y < (sy + 8) && x >= sx && x < (sx + 8)) begin
          stile = oam[i*4 + 1];
          sattr = oam[i*4 + 2];
          srow = y - sy;
          scol = x - sx;
          if (sattr[7]) srow = 7 - srow;
          if (sattr[6]) scol = 7 - scol;
          spat = (ctrl[3] ? 13'h1000 : 13'h0000) + {4'd0, stile, 4'd0} + srow[3:0];
          sbit = 7 - scol[2:0];
          scolor = {chr[spat[12:0] + 13'd8][sbit], chr[spat[12:0]][sbit]};
          if (scolor != 2'b00) begin
            sprite_data = pal[8'h10 + ((sattr & 2'b11) << 2) + scolor];
            if (i == 0 && bg_opaque) sprite0_hit = 1'b1;
            if (!sattr[5] || !bg_opaque) pixel_index = sprite_data;
            i = 64;
          end
        end
      end
    end
  end

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
      vaddr <= 15'h0000; scroll_x <= 8'h00; scroll_y <= 8'h00;
      read_buf <= 8'h00; w_toggle <= 1'b0;
      nmi_level <= 1'b0;
      for (int i = 0; i < 8192; i++) chr[i] <= 8'h00;
      for (int i = 0; i < 2048; i++) vram[i] <= 8'h00;
      for (int i = 0; i < 256; i++) oam[i] <= 8'h00;
      for (int i = 0; i < 32; i++) pal[i] <= 8'h00;
    end else begin
      if (dma_we) oam[dma_addr] <= dma_wdata;
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
            3'd5: begin
              if (!w_toggle) scroll_x <= wdata;
              else           scroll_y <= wdata;
              w_toggle <= ~w_toggle;
            end
            3'd6: begin
              if (!w_toggle) vaddr[14:8] <= wdata[6:0];
              else           vaddr[7:0]  <= wdata;
              w_toggle <= ~w_toggle;
            end
            3'd7: begin
              if (vaddr[13:8] == 6'h3F) pal[vaddr[4:0]] <= wdata;
              else if (vaddr < 15'h2000) chr[vaddr[12:0]] <= wdata;
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
              else if (vaddr < 15'h2000) read_buf <= chr[vaddr[12:0]];
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
