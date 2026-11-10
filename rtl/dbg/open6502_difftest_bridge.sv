module open6502_difftest_bridge (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        valid,
  input  logic [1:0]  kind,
  input  logic [63:0] tick,
  input  logic [15:0] addr,
  input  logic [7:0]  data,
  input  logic        we,
  input  logic [7:0]  source,
  output logic        dbg_valid,
  output logic [63:0] dbg_payload
);
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      dbg_valid <= 1'b0;
      dbg_payload <= '0;
    end else begin
      dbg_valid <= valid;
      if (valid) begin
        dbg_payload <= {kind, tick[47:0], addr, data, we, source};
      end
    end
  end
endmodule
