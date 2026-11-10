VERILATOR ?= verilator
TOP := open6502_top
RTL := open6502_pkg.sv open6502_top.sv video/open6502_video_timing.sv \
       input/open6502_controller.sv dbg/open6502_difftest_bridge.sv \
       bus/open6502_bus.sv bus/open6502_wram.sv

lint:
	$(VERILATOR) --lint-only -Wall -Wno-fatal --top-module $(TOP) $(RTL)

TB := ../tb/open6502_tb.sv

IV ?= iverilog
BUS_TB := ../tb/open6502_bus_tb.sv

sim: $(TB) $(BUS_TB) $(RTL)
	$(IV) -g2012 -o open6502_tb.vvp $(RTL) $(TB)
	vvp open6502_tb.vvp
	$(IV) -g2012 -o open6502_bus_tb.vvp bus/open6502_bus.sv \
	  bus/open6502_wram.sv $(BUS_TB)
	vvp open6502_bus_tb.vvp

sim-verilator: $(TB) $(RTL)
	$(VERILATOR) --binary -Wall -Wno-fatal --top-module open6502_tb \
	  -Mdir obj_dir $(RTL) $(TB)
	./obj_dir/Vopen6502_tb

clean:
	rm -rf obj_dir open6502_tb.vvp
.PHONY: lint sim clean
