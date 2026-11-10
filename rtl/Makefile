VERILATOR ?= verilator
TOP := open6502_top
RTL := open6502_pkg.sv open6502_top.sv video/open6502_video_timing.sv \
       input/open6502_controller.sv dbg/open6502_difftest_bridge.sv \
       bus/open6502_bus.sv bus/open6502_wram.sv clocks/open6502_clocks.sv \
       cpu/open6502_cpu.sv ppu/open6502_ppu.sv dma/open6502_oam_dma.sv \
       apu/open6502_apu.sv apu/open6502_dmc.sv cart/open6502_mapper.sv \
       fpga/open6502_fpga_top.sv

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
RMW_TB := ../tb/open6502_rmw_tb.sv
BRK_TB := ../tb/open6502_brk_tb.sv
IRQ_TB := ../tb/open6502_irq_tb.sv
INC_TB := ../tb/open6502_inc_tb.sv
PPU_TB := ../tb/open6502_ppu_tb.sv
PPU_PIX_TB := ../tb/open6502_ppu_pixel_tb.sv
PPU_SPR_TB := ../tb/open6502_ppu_sprite_tb.sv
DMA_TB := ../tb/open6502_oam_dma_tb.sv
DMA_SOC_TB := ../tb/open6502_oam_soc_tb.sv
PPU_SCROLL_TB := ../tb/open6502_ppu_scroll_tb.sv
PPU_OVF_TB := ../tb/open6502_ppu_overflow_tb.sv
APU_TB := ../tb/open6502_apu_tb.sv
APU_SWEEP_TB := ../tb/open6502_apu_sweep_tb.sv
DMC_TB := ../tb/open6502_dmc_tb.sv
MAP_TB := ../tb/open6502_mapper_tb.sv
JMPI_TB := ../tb/open6502_jmp_ind_tb.sv

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
	$(IV) -g2012 -o open6502_rmw_tb.vvp cpu/open6502_cpu.sv $(RMW_TB)
	vvp open6502_rmw_tb.vvp
	$(IV) -g2012 -o open6502_brk_tb.vvp cpu/open6502_cpu.sv $(BRK_TB)
	vvp open6502_brk_tb.vvp
	$(IV) -g2012 -o open6502_irq_tb.vvp cpu/open6502_cpu.sv $(IRQ_TB)
	vvp open6502_irq_tb.vvp
	$(IV) -g2012 -o open6502_inc_tb.vvp cpu/open6502_cpu.sv $(INC_TB)
	vvp open6502_inc_tb.vvp
	$(IV) -g2012 -o open6502_ppu_tb.vvp ppu/open6502_ppu.sv $(PPU_TB)
	vvp open6502_ppu_tb.vvp
	$(IV) -g2012 -o open6502_ppu_pixel_tb.vvp ppu/open6502_ppu.sv $(PPU_PIX_TB)
	vvp open6502_ppu_pixel_tb.vvp
	$(IV) -g2012 -o open6502_ppu_sprite_tb.vvp ppu/open6502_ppu.sv $(PPU_SPR_TB)
	vvp open6502_ppu_sprite_tb.vvp
	$(IV) -g2012 -o open6502_oam_dma_tb.vvp dma/open6502_oam_dma.sv $(DMA_TB)
	vvp open6502_oam_dma_tb.vvp
	$(IV) -g2012 -o open6502_oam_soc_tb.vvp $(RTL) $(DMA_SOC_TB)
	vvp open6502_oam_soc_tb.vvp
	$(IV) -g2012 -o open6502_ppu_scroll_tb.vvp ppu/open6502_ppu.sv $(PPU_SCROLL_TB)
	vvp open6502_ppu_scroll_tb.vvp
	$(IV) -g2012 -o open6502_ppu_overflow_tb.vvp ppu/open6502_ppu.sv $(PPU_OVF_TB)
	vvp open6502_ppu_overflow_tb.vvp
	$(IV) -g2012 -o open6502_apu_tb.vvp apu/open6502_apu.sv $(APU_TB)
	vvp open6502_apu_tb.vvp
	$(IV) -g2012 -o open6502_apu_sweep_tb.vvp apu/open6502_apu.sv $(APU_SWEEP_TB)
	vvp open6502_apu_sweep_tb.vvp
	$(IV) -g2012 -o open6502_dmc_tb.vvp apu/open6502_dmc.sv $(DMC_TB)
	vvp open6502_dmc_tb.vvp
	$(IV) -g2012 -o open6502_mapper_tb.vvp cart/open6502_mapper.sv $(MAP_TB)
	vvp open6502_mapper_tb.vvp
	$(IV) -g2012 -o open6502_jmp_ind_tb.vvp cpu/open6502_cpu.sv $(JMPI_TB)
	vvp open6502_jmp_ind_tb.vvp

sim-verilator: $(TB) $(RTL)
	$(VERILATOR) --binary -Wall -Wno-fatal --top-module open6502_tb \
	  -Mdir obj_dir $(RTL) $(TB)
	./obj_dir/Vopen6502_tb

clean:
	rm -rf obj_dir open6502_tb.vvp
.PHONY: lint sim clean
