module open6502_bus (
  input  logic [15:0] cpu_addr,
  input  logic        cpu_we,
  input  logic [7:0]  cpu_wdata,
  input  logic [7:0]  ram_rdata,
  input  logic [7:0]  ppu_rdata,
  input  logic [7:0]  apu_rdata,
  input  logic [7:0]  cart_rdata,
  output logic        ram_sel,
  output logic        ppu_sel,
  output logic        apu_sel,
  output logic        cart_sel,
  output logic        ram_we,
  output logic        ppu_we,
  output logic        apu_we,
  output logic        cart_we,
  output logic [10:0] ram_addr,
  output logic [7:0]  cpu_rdata
);
  always_comb begin
    ram_sel  = 1'b0;
    ppu_sel  = 1'b0;
    apu_sel  = 1'b0;
    cart_sel = 1'b0;
    ram_addr = 11'd0;
    if (cpu_addr < 16'h2000) begin
      ram_sel  = 1'b1;
      ram_addr = cpu_addr[10:0];
    end else if (cpu_addr < 16'h4000) begin
      ppu_sel = 1'b1;
    end else if (cpu_addr < 16'h4020) begin
      apu_sel = 1'b1;
    end else if (cpu_addr >= 16'h6000 && cpu_addr < 16'h8000) begin
      ram_sel  = 1'b1;
      ram_addr = cpu_addr[10:0];
    end else if (cpu_addr >= 16'h8000) begin
      cart_sel = 1'b1;
    end
  end

  assign ram_we  = ram_sel  && cpu_we;
  assign ppu_we  = ppu_sel  && cpu_we;
  assign apu_we  = apu_sel  && cpu_we;
  assign cart_we = cart_sel && cpu_we;

  always_comb begin
    if (ram_sel)       cpu_rdata = ram_rdata;
    else if (ppu_sel)  cpu_rdata = ppu_rdata;
    else if (apu_sel)  cpu_rdata = apu_rdata;
    else if (cart_sel) cpu_rdata = cart_rdata;
    else               cpu_rdata = 8'h00;
  end
endmodule
