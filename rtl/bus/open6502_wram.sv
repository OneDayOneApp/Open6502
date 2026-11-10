module open6502_wram (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        we,
  input  logic [10:0] addr,
  input  logic [7:0]  wdata,
  output logic [7:0]  rdata
);
  logic [7:0] mem [0:2047];

  always_ff @(posedge clk) begin
    if (we) mem[addr] <= wdata;
  end

  assign rdata = mem[addr];
endmodule
