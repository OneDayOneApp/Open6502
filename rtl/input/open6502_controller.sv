module open6502_controller (
  input  logic       clk,
  input  logic       rst_n,
  input  logic       strobe,
  input  logic       shift_en,
  input  logic [7:0] buttons,
  output logic       serial
);
  logic [7:0] shreg;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      shreg <= 8'h00;
      serial <= 1'b0;
    end else if (strobe) begin
      shreg <= buttons;
      serial <= buttons[0];
    end else if (shift_en) begin
      serial <= shreg[0];
      shreg <= {1'b0, shreg[7:1]};
    end
  end
endmodule
