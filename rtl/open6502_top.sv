  function automatic logic [11:0] pal444(input logic [5:0] idx);
    case (idx)
      6'h00: pal444 = 12'h666;
      6'h01: pal444 = 12'h028;
      6'h02: pal444 = 12'h11A;
      6'h03: pal444 = 12'h30A;
      6'h04: pal444 = 12'h507;
      6'h05: pal444 = 12'h604;
      6'h06: pal444 = 12'h600;
      6'h07: pal444 = 12'h510;
      6'h08: pal444 = 12'h330;
      6'h09: pal444 = 12'h040;
      6'h0A: pal444 = 12'h050;
      6'h0B: pal444 = 12'h040;
      6'h0C: pal444 = 12'h044;
      6'h0D: pal444 = 12'h000;
      6'h0E: pal444 = 12'h000;
      6'h0F: pal444 = 12'h000;
      6'h10: pal444 = 12'hAAA;
      6'h11: pal444 = 12'h15D;
      6'h12: pal444 = 12'h44F;
      6'h13: pal444 = 12'h72F;
      6'h14: pal444 = 12'hA1C;
      6'h15: pal444 = 12'hB17;
      6'h16: pal444 = 12'hB32;
      6'h17: pal444 = 12'h940;
      6'h18: pal444 = 12'h660;
      6'h19: pal444 = 12'h380;
      6'h1A: pal444 = 12'h090;
      6'h1B: pal444 = 12'h083;
      6'h1C: pal444 = 12'h078;
      6'h1D: pal444 = 12'h000;
      6'h1E: pal444 = 12'h000;
      6'h1F: pal444 = 12'h000;
      6'h20: pal444 = 12'hFFF;
      6'h21: pal444 = 12'h6BF;
      6'h22: pal444 = 12'h99F;
      6'h23: pal444 = 12'hC7F;
      6'h24: pal444 = 12'hF6F;
      6'h25: pal444 = 12'hF6C;
      6'h26: pal444 = 12'hF87;
      6'h27: pal444 = 12'hE92;
      6'h28: pal444 = 12'hBB0;
      6'h29: pal444 = 12'h8D0;
      6'h2A: pal444 = 12'h5E3;
      6'h2B: pal444 = 12'h4E8;
      6'h2C: pal444 = 12'h4CD;
      6'h2D: pal444 = 12'h444;
      6'h2E: pal444 = 12'h000;
      6'h2F: pal444 = 12'h000;
      6'h30: pal444 = 12'hFFF;
      6'h31: pal444 = 12'hCEF;
      6'h32: pal444 = 12'hDDF;
      6'h33: pal444 = 12'hECF;
      6'h34: pal444 = 12'hFCF;
      6'h35: pal444 = 12'hFCE;
      6'h36: pal444 = 12'hFCC;
      6'h37: pal444 = 12'hFDA;
      6'h38: pal444 = 12'hEE9;
      6'h39: pal444 = 12'hCE9;
      6'h3A: pal444 = 12'hBFA;
      6'h3B: pal444 = 12'hBFC;
      6'h3C: pal444 = 12'hBEF;
      6'h3D: pal444 = 12'hBBB;
      6'h3E: pal444 = 12'h000;
      6'h3F: pal444 = 12'h000;
      default: pal444 = 12'h000;
    endcase
  endfunction
module open6502_top #(
  parameter logic [2:0] MAPPER = 3'd0
) (
  input  logic        clk_21m,
  input  logic        rst_n,

  output logic        vid_pclk,
  output logic        vid_de,
  output logic        vid_hsync,
  output logic        vid_vsync,
  output logic [11:0] vid_rgb,

  output logic        aud_valid,
  output logic signed [15:0] aud_sample,

  input  logic [12:0] chr_load_addr,
  input  logic [7:0]  chr_load_data,
  input  logic        chr_load_we,

  input  logic [7:0]  joy1,
  input  logic [7:0]  joy2,

  output logic [15:0] cart_addr,
  output logic [7:0]  cart_wdata,
  input  logic [7:0]  cart_rdata,
  output logic        cart_we,
  input  logic        cart_irq,

  output logic        dbg_valid,
  output logic [63:0] dbg_payload,
  output logic [15:0] dbg_cpu_pc,
  output logic [7:0]  dbg_ppu_ctrl,
  output logic [7:0]  dbg_ppu_mask,
  output logic [7:0]  dbg_ppu_status,
  output logic        dbg_dma_busy,
  output logic [1:0]  dbg_dma_state,
  output logic [7:0]  dbg_dma_count,
  output logic [7:0]  dbg_ppu_pixel,
  output logic [7:0]  dbg_pal0,
  output logic [7:0]  dbg_chr0,
  output logic [7:0]  dbg_pal1,
  output logic [7:0]  dbg_pal2,
  output logic [8:0]  dbg_ppu_x,
  output logic [8:0]  dbg_ppu_y
);

  logic        rst_sync_n;
  logic        cpu_en;
  logic        ppu_en;
  logic        aud_en;
  logic [8:0]  ppu_x;
  logic [8:0]  ppu_y;
  logic        frame_tick;
  logic [15:0] cpu_addr;
  logic [7:0]  cpu_wdata;
  logic [7:0]  cpu_rdata;
  logic [7:0]  cpu_rdata_bus;
  logic [7:0]  joy1_shreg, joy2_shreg;
  logic [7:0]  joy_rdata;
  logic        cpu_we;
  logic [7:0]  ram_rdata;
  logic [10:0] ram_addr;
  logic        ram_sel, ppu_sel, apu_sel, cart_sel;
  logic        ram_we, ppu_we, apu_we, cart_we_i;
  logic [7:0]  ppu_rdata;
  logic [7:0]  apu_rdata;
  logic signed [15:0] apu_sample;
  logic        apu_irq;
  logic        apu_dmc_enable;
  logic        dmc_cs, dmc_irq, dmc_dma_req;
  logic [2:0]  dmc_stall;
  logic [15:0] dmc_addr;
  logic [7:0]  dmc_data;
  logic [6:0]  dmc_out;
  logic signed [16:0] audio_mix;
  logic        ppu_nmi;
  logic [7:0]  ppu_pixel;
  logic        ppu_sprite0;
  logic [15:0] bus_addr;
  logic        bus_we;
  logic        dma_busy;
  logic        dma_start;
  logic        dma_req_d;
  logic        dma_lock;
  logic        cpu_ce;
  logic [15:0] dma_mem_addr;
  logic        dma_oam_we;
  logic [7:0]  dma_oam_addr, dma_oam_wdata;
  logic [14:0] mapper_prg;
  logic [12:0] mapper_chr;
  logic        mapper_mirror, mapper_irq;
  logic        joy1_serial;
  logic        joy2_serial;
  logic        dbg_valid_i;
  logic [63:0] dbg_payload_i;
  logic [7:0]  dbg_a_i;

  open6502_clocks u_clocks (
    .clk_21m(clk_21m), .rst_n(rst_n),
    .rst_sync_n(rst_sync_n), .cpu_en(cpu_en),
    .ppu_en(ppu_en), .aud_en(aud_en)
  );

  open6502_video_timing u_video (
    .clk(clk_21m), .rst_n(rst_sync_n), .ppu_en(ppu_en),
    .x(ppu_x), .y(ppu_y),
    .de(vid_de), .hsync(vid_hsync), .vsync(vid_vsync),
    .frame_tick(frame_tick)
  );

  always_ff @(posedge clk_21m or negedge rst_sync_n) begin
    if (!rst_sync_n) begin
      joy1_shreg <= 8'h00;
      joy2_shreg <= 8'h00;
    end else if (cpu_ce && (cpu_addr == 16'h4016) && cpu_we) begin
      joy1_shreg <= joy1;
      joy2_shreg <= joy2;
    end else if (cpu_ce && !cpu_we && (cpu_addr == 16'h4016)) begin
      joy1_shreg <= {1'b0, joy1_shreg[7:1]};
    end else if (cpu_ce && !cpu_we && (cpu_addr == 16'h4017)) begin
      joy2_shreg <= {1'b0, joy2_shreg[7:1]};
    end
  end
  assign joy_rdata = (cpu_addr == 16'h4017) ? {7'b0, joy2_shreg[0]} :
                                                   {7'b0, joy1_shreg[0]};
  assign cpu_rdata = ((cpu_addr == 16'h4016) || (cpu_addr == 16'h4017)) ? joy_rdata : cpu_rdata_bus;

  assign cpu_ce = cpu_en && !dma_busy && !dmc_stall;

  open6502_cpu u_cpu (
    .clk(clk_21m), .rst_n(rst_sync_n), .cpu_en(cpu_ce),
    .bus_rdata(cpu_rdata), .irq_n(~(apu_irq | dmc_irq | mapper_irq)),
    .nmi_n(~ppu_nmi),
    .bus_addr(cpu_addr), .bus_wdata(cpu_wdata), .bus_we(cpu_we),
    .dbg_pc(dbg_cpu_pc), .dbg_a(dbg_a_i), .dbg_x(), .dbg_y(), .dbg_sp(), .dbg_p(), .dbg_state()
  );

  open6502_bus u_bus (
    .cpu_addr(bus_addr), .cpu_we(bus_we), .cpu_wdata(cpu_wdata),
    .ram_rdata(ram_rdata), .ppu_rdata(ppu_rdata), .apu_rdata(apu_rdata),
    .cart_rdata(cart_rdata),
    .ram_sel(ram_sel), .ppu_sel(ppu_sel), .apu_sel(apu_sel),
    .cart_sel(cart_sel), .ram_we(ram_we), .ppu_we(ppu_we),
    .apu_we(apu_we), .cart_we(cart_we_i), .ram_addr(ram_addr),
    .cpu_rdata(cpu_rdata_bus)
  );

  open6502_wram u_ram (
    .clk(clk_21m), .rst_n(rst_sync_n), .we(ram_we),
    .addr(ram_addr), .wdata(cpu_wdata), .rdata(ram_rdata)
  );

  open6502_ppu u_ppu (
    .clk(clk_21m), .rst_n(rst_sync_n), .ppu_en(ppu_en),
    .x(ppu_x), .y(ppu_y),
    .cs(ppu_sel && cpu_ce), .we(ppu_we), .addr(cpu_addr[2:0]),
    .wdata(cpu_wdata), .dma_we(dma_oam_we), .dma_addr(dma_oam_addr),
    .dma_wdata(dma_oam_wdata), .rdata(ppu_rdata), .pixel_index(ppu_pixel),
    .chr_load_we(chr_load_we), .chr_load_addr(chr_load_addr),
    .chr_load_data(chr_load_data),
    .sprite0_hit(ppu_sprite0), .nmi(ppu_nmi)
  );

  open6502_mapper u_mapper (
    .clk(clk_21m), .rst_n(rst_sync_n), .mapper(MAPPER),
    .cpu_addr(bus_addr), .cpu_we(bus_we), .cpu_wdata(cpu_wdata),
    .ppu_a12(ppu_x[8]), .prg_addr(mapper_prg), .chr_addr(mapper_chr),
    .mirroring(mapper_mirror), .irq(mapper_irq)
  );

  open6502_apu u_apu (
    .clk(clk_21m), .rst_n(rst_sync_n), .cpu_en(cpu_en),
    .cs(apu_sel), .we(apu_we), .addr(cpu_addr[4:0]),
    .wdata(cpu_wdata), .rdata(apu_rdata), .sample(apu_sample), .irq(apu_irq),
    .dmc_enable(apu_dmc_enable)
  );

  assign dmc_cs = apu_sel && (cpu_addr >= 16'h4010) && (cpu_addr <= 16'h4013);
  assign dmc_data = (dmc_addr >= 16'h8000) ? cart_rdata : 8'h00;

  open6502_dmc u_dmc (
    .clk(clk_21m), .rst_n(rst_sync_n), .cpu_en(cpu_en),
    .enable(apu_dmc_enable), .cs(dmc_cs), .we(apu_we),
    .addr(cpu_addr[1:0]), .wdata(cpu_wdata), .sample_data(dmc_data),
    .sample_addr(dmc_addr), .sample_out(dmc_out), .irq(dmc_irq), .active(),
    .dma_req(dmc_dma_req)
  );

  open6502_oam_dma u_dma (
    .clk(clk_21m), .rst_n(rst_sync_n), .start(dma_start),
    .page(cpu_wdata), .mem_rdata(cpu_rdata), .mem_addr(dma_mem_addr),
    .oam_we(dma_oam_we), .oam_addr(dma_oam_addr),
    .oam_wdata(dma_oam_wdata), .busy(dma_busy),
    .dbg_state(dbg_dma_state), .dbg_count(dbg_dma_count)
  );

  assign bus_addr = dma_busy ? dma_mem_addr : cpu_addr;
  assign bus_we   = dma_busy ? 1'b0 : cpu_we;
  always_ff @(posedge clk_21m or negedge rst_sync_n) begin
    if (!rst_sync_n) begin
      dma_req_d <= 1'b0;
      dma_lock <= 1'b0;
    end else begin
      dma_req_d <= apu_sel && cpu_we && (cpu_addr == 16'h4014);
      if (dma_busy) dma_lock <= 1'b1;
      else if (!(apu_sel && cpu_we && (cpu_addr == 16'h4014))) dma_lock <= 1'b0;
    end
  end
  assign dma_start = apu_sel && cpu_we && (cpu_addr == 16'h4014) &&
                     !dma_req_d && !dma_lock;

  always_ff @(posedge clk_21m) begin
    if (!rst_sync_n) dmc_stall <= 3'd0;
    else if (dmc_dma_req) dmc_stall <= 3'd4;
    else if (dmc_stall != 0) dmc_stall <= dmc_stall - 3'd1;
  end

  open6502_difftest_bridge u_dbg (
    .clk(clk_21m), .rst_n(rst_sync_n),
    .valid(1'b0), .kind(2'd0), .tick(64'd0),
    .addr(16'd0), .data(8'd0), .we(1'b0), .source(8'd0),
    .dbg_valid(dbg_valid_i), .dbg_payload(dbg_payload_i)
  );

  assign vid_pclk = clk_21m;
  assign vid_rgb = pal444(ppu_pixel[5:0]);
  assign aud_valid = aud_en;
  assign audio_mix = apu_sample + ((dmc_out - 7'd64) * 16'sd256);
  assign aud_sample = audio_mix[15:0];
  assign cart_addr = (MAPPER == 3'd0) ? bus_addr : {1'b0, mapper_prg};
  assign cart_wdata = cpu_wdata;
  assign cart_we = cart_we_i;
  assign dbg_valid = dbg_valid_i;
  assign dbg_payload = dbg_payload_i;
  assign dbg_ppu_ctrl = u_ppu.ctrl;
  assign dbg_ppu_mask = u_ppu.mask;
  assign dbg_ppu_status = u_ppu.status;
  assign dbg_dma_busy = dma_busy;
  assign dbg_ppu_pixel = ppu_pixel;
  assign dbg_pal0 = u_ppu.pal[0];
  assign dbg_chr0 = u_ppu.chr[0];
  assign dbg_pal1 = u_ppu.pal[1];
  assign dbg_pal2 = u_ppu.pal[2];
  assign dbg_ppu_x = ppu_x;
  assign dbg_ppu_y = ppu_y;

  // Keep controller/cart/joy signals observable while the CPU/PPU/APU
  // implementations are brought up in later phases.
  logic unused;
  assign unused = ^{joy1_serial, joy2_serial, cart_irq, frame_tick,
                    ppu_x, ppu_y, ram_sel, ppu_sel, apu_sel, cart_sel,
                    ppu_we, apu_we, mapper_chr, mapper_mirror, mapper_irq};
endmodule
