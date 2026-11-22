#!/usr/bin/env python3
"""Co-simulation: Lean ISA model vs. the SystemVerilog RTL.

For a small set of programs this script

1. builds a 64 KiB image (the same bytes as the Lean tests),
2. runs it on the RTL with Icarus Verilog, stopping at the fetch-boundary trap,
3. runs it on the Lean model with `lake exe open6502-run`,
4. compares the final `pc a x y p sp`.

It complements the Lean proofs: the proofs relate the Lean FSM model to the
Lean ISA model, and this script checks the Lean ISA model against the actual
RTL on executable programs.

Usage: python3 formal/scripts/cosim.py   (from the repository root)
"""

import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FORMAL = os.path.join(ROOT, "formal")
BUILD = os.path.join(FORMAL, "build", "cosim")
RTL = os.path.join(ROOT, "rtl", "cpu", "open6502_cpu.sv")

TB = r"""`timescale 1ns/1ps
module cosim_tb;
  logic clk = 0, rst_n = 0, cpu_en = 0;
  logic [7:0] mem [0:65535];
  logic [7:0] rdata;
  logic [15:0] addr;
  logic [7:0] wdata;
  logic we;
  logic [15:0] pc;
  logic [7:0] a, x, y, sp, p;
  logic [5:0] state;

  open6502_cpu dut (
    .clk(clk), .rst_n(rst_n), .cpu_en(cpu_en),
    .bus_rdata(rdata), .irq_n(1'b1), .nmi_n(1'b1),
    .bus_addr(addr), .bus_wdata(wdata), .bus_we(we),
    .dbg_pc(pc), .dbg_a(a), .dbg_x(x), .dbg_y(y), .dbg_sp(sp), .dbg_p(p),
    .dbg_state(state)
  );

  always #10 clk = ~clk;
  assign rdata = mem[addr];

  always @(posedge clk)
    if (rst_n && cpu_en && we) mem[addr] <= wdata;

  logic [15:0] pc_prev;
  initial begin
    $readmemh("image.hex", mem);
    repeat (4) @(posedge clk);
    rst_n = 1;
    pc_prev = 16'hFFFF;
    for (int i = 0; i < __MAXCYC__; i++) begin
      @(posedge clk); cpu_en = 1;
      @(posedge clk); #1; cpu_en = 0;
      if (state == 6'd2 && pc == pc_prev) begin
        $display("RTL %04h %02h %02h %02h %02h %02h", pc, a, x, y, p, sp);
        $finish;
      end
      if (state == 6'd2) pc_prev = pc;
    end
    $display("TIMEOUT %04h %02h %02h %02h %02h %02h", pc, a, x, y, p, sp);
    $finish;
  end
endmodule
"""

# (name, [(address, byte), ...], entry, max cycles)
PROGRAMS = [
    ("adc",
     [(0x8000, 0x38), (0x8001, 0xA9), (0x8002, 0x00), (0x8003, 0x69), (0x8004, 0x00),
      (0x8005, 0x8D), (0x8006, 0x00), (0x8007, 0x02), (0x8008, 0x4C), (0x8009, 0x08),
      (0x800A, 0x80), (0xFFFC, 0x00), (0xFFFD, 0x80)],
     0x8000, 200),
    ("decimal",
     [(0x8000, 0xF8), (0x8001, 0xA9), (0x8002, 0x99), (0x8003, 0x18), (0x8004, 0x69),
      (0x8005, 0x99), (0x8006, 0x8D), (0x8007, 0x00), (0x8008, 0x02), (0x8009, 0xA9),
      (0x800A, 0x00), (0x800B, 0x38), (0x800C, 0xE9), (0x800D, 0x01), (0x800E, 0x8D),
      (0x800F, 0x01), (0x8010, 0x02), (0x8011, 0x4C), (0x8012, 0x11), (0x8013, 0x80),
      (0xFFFC, 0x00), (0xFFFD, 0x80)],
     0x8000, 400),
    ("jsr",
     [(0x8000, 0x20), (0x8001, 0x10), (0x8002, 0x80), (0x8003, 0x8D), (0x8004, 0x00),
      (0x8005, 0x02), (0x8006, 0x4C), (0x8007, 0x06), (0x8008, 0x80),
      (0x8010, 0xA9), (0x8011, 0x42), (0x8012, 0x60),
      (0xFFFC, 0x00), (0xFFFD, 0x80)],
     0x8000, 300),
    ("branch",
     [(0x8000, 0xA2), (0x8001, 0x00), (0x8002, 0xE8), (0x8003, 0xE0), (0x8004, 0x05),
      (0x8005, 0xD0), (0x8006, 0xFB), (0x8007, 0x4C), (0x8008, 0x07), (0x8009, 0x80),
      (0xFFFC, 0x00), (0xFFFD, 0x80)],
     0x8000, 400),
]


def build_image(entries):
    mem = bytearray([0xEA] * 65536)
    for addr, byte in entries:
        mem[addr] = byte
    return mem


def write_hex(mem, path):
    with open(path, "w") as f:
        for b in mem:
            f.write(f"{b:02X}\n")


def run_rtl(mem, max_cyc):
    write_hex(mem, os.path.join(BUILD, "image.hex"))
    tb = TB.replace("__MAXCYC__", str(max_cyc))
    with open(os.path.join(BUILD, "cosim_tb.sv"), "w") as f:
        f.write(tb)
    exe = os.path.join(BUILD, "sim.vvp")
    subprocess.run(
        ["iverilog", "-g2012", "-o", exe,
         os.path.join(BUILD, "cosim_tb.sv"), RTL],
        check=True, cwd=BUILD,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    out = subprocess.run(["vvp", exe], check=True, cwd=BUILD,
                         capture_output=True, text=True).stdout
    m = re.search(r"^(RTL|TIMEOUT)\s+([0-9a-f]{4})\s+([0-9a-f]{2})\s+([0-9a-f]{2})"
                  r"\s+([0-9a-f]{2})\s+([0-9a-f]{2})\s+([0-9a-f]{2})",
                  out, re.MULTILINE)
    if not m:
        raise SystemExit(f"could not parse RTL output:\n{out}")
    if m.group(1) == "TIMEOUT":
        raise SystemExit(f"RTL timed out at pc={m.group(2)}")
    return tuple(int(x, 16) for x in m.groups()[1:])


def run_lean(mem, entry):
    bin_path = os.path.join(BUILD, "image.bin")
    with open(bin_path, "wb") as f:
        f.write(mem)
    out = subprocess.run(
        ["lake", "exe", "open6502-run", bin_path,
         "--entry", hex(entry), "--steps", "2000000"],
        cwd=FORMAL, capture_output=True, text=True,
    )
    if out.returncode != 0:
        raise SystemExit(f"lean runner failed:\n{out.stderr[-2000:]}")
    m = re.search(r"final pc=([0-9A-F]+) a=([0-9A-F]+) x=([0-9A-F]+) "
                  r"y=([0-9A-F]+) p=([0-9A-F]+) sp=([0-9A-F]+)", out.stdout)
    if not m:
        raise SystemExit(f"could not parse Lean output:\n{out.stdout}")
    return tuple(int(x, 16) for x in m.groups())


def main():
    os.makedirs(BUILD, exist_ok=True)
    failures = 0
    for name, entries, entry, max_cyc in PROGRAMS:
        mem = build_image(entries)
        rtl = run_rtl(mem, max_cyc)
        lean = run_lean(mem, entry)
        ok = rtl == lean
        fmt = lambda t: "pc=%04x a=%02x x=%02x y=%02x p=%02x sp=%02x" % t
        print(f"{'ok  ' if ok else 'FAIL'} {name:8} {fmt(rtl)}")
        if not ok:
            failures += 1
            print(f"     lean     {fmt(lean)}")
    if failures:
        print(f"{failures} co-simulation mismatch(es)")
        return 1
    print(f"all {len(PROGRAMS)} programs match the RTL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
