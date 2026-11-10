VERILATOR ?= verilator
TOP := open6502_top
RTL := open6502_pkg.sv open6502_top.sv video/open6502_video_timing.sv \
       input/open6502_controller.sv dbg/open6502_difftest_bridge.sv \
       bus/open6502_bus.sv bus/open6502_wram.sv clocks/open6502_clocks.sv \
       cpu/open6502_cpu.sv

lint:
	$(VERILATOR) --lint-only -Wall -Wno-fatal --top-module $(TOP) $(RTL)

TB := ../tb/open6502_tb.sv

IV ?= iverilog
BUS_TB := ../tb/open6502_bus_tb.sv
CLK_TB := ../tb/open6502_clocks_tb.sv
CPU_TB := ../tb/open6502_cpu_tb.sv
SOC_TB := ../tb/open6502_soc_tb.sv
JSR_TB := ../tb/open6502_jsr_tb.sv
STACK_TB := ../tb/open6502_stack_tb.sv

sim: $(TB) $(BUS_TB) $(RTL)
	$(IV) -g2012 -o open6502_tb.vvp $(RTL) $(TB)
	vvp open6502_tb.vvp
	$(IV) -g2012 -o open6502_bus_tb.vvp bus/open6502_bus.sv \
	  bus/open6502_wram.sv $(BUS_TB)
	vvp open6502_bus_tb.vvp
	$(IV) -g2012 -o open6502_clocks_tb.vvp open6502_pkg.sv clocks/open6502_clocks.sv $(CLK_TB)
	vvp open6502_clocks_tb.vvp
	$(IV) -g2012 -o open6502_cpu_tb.vvp cpu/open6502_cpu.sv $(CPU_TB)
	vvp open6502_cpu_tb.vvp
	$(IV) -g2012 -o open6502_soc_tb.vvp $(RTL) $(SOC_TB)
	vvp open6502_soc_tb.vvp
	$(IV) -g2012 -o open6502_jsr_tb.vvp cpu/open6502_cpu.sv $(JSR_TB)
	vvp open6502_jsr_tb.vvp
	$(IV) -g2012 -o open6502_stack_tb.vvp cpu/open6502_cpu.sv $(STACK_TB)
	vvp open6502_stack_tb.vvp

sim-verilator: $(TB) $(RTL)
	$(VERILATOR) --binary -Wall -Wno-fatal --top-module open6502_tb \
	  -Mdir obj_dir $(RTL) $(TB)
	./obj_dir/Vopen6502_tb

clean:
	rm -rf obj_dir open6502_tb.vvp
.PHONY: lint sim clean
