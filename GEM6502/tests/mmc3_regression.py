#!/usr/bin/env python3
"""Bundled MMC3 CPU/clock differential and START gameplay smoke regression."""
import itertools
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GEM = ROOT / 'GEM6502/gem6502'
NEMU = ROOT / 'NEMU/build/6502EMU'


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=120, **kwargs).stdout


def compare(npath, gpath):
    previous_phase = ['0', '0']
    count = 0
    with npath.open() as nf, gpath.open() as gf:
        for count, (nl, gl) in enumerate(itertools.zip_longest(nf, gf), 1):
            assert nl and gl, f'trace length mismatch at row {count}'
            n, g = nl.split(), gl.split()
            # NEMU records before advancing the PPU; GEM records afterwards.
            assert n[:6] == g[:6], f'CPU mismatch at row {count}: {n} / {g}'
            assert n[6] == g[12], f'CPU cycle mismatch at row {count}'
            assert n[7:9] == previous_phase, f'PPU phase mismatch at row {count}'
            previous_phase = g[13:15]
    assert count > 100000, 'trace too short'
    return count


def check_game(output, frames):
    assert re.search(rf'game: frames={frames}\b', output), output
    assert 'illegal=0' in output and 'stop=running' in output, output


with tempfile.TemporaryDirectory(prefix='gem6502-mmc3-') as directory:
    work = Path(directory)
    for name in ('file-fixers', 'stallar'):
        rom = ROOT / 'roms' / (name + '.nes')
        nt, gt = work / 'nemu.trace', work / 'gem.trace'
        env = dict(os.environ, NEMU_NES_TRACE=str(nt))
        run([NEMU, '--headless', '--no-audio', '--frames=60', rom], env=env)
        output = run([GEM, 'game', rom, 60, f'--trace-state={gt}'])
        check_game(output, 60)
        print(f'PASS: {name} 60 frames, {compare(nt, gt)} CPU/cycle/PPU trace rows match')
        image = work / (name + '.ppm')
        output = run([GEM, 'game', rom, 600, image, '--start'])
        check_game(output, 600)
        data = image.read_bytes().split(b'\n', 3)[3]
        assert len(data) == 256 * 240 * 3
        assert len(set(zip(data[::3], data[1::3], data[2::3]))) > 3, 'blank gameplay'
        print(f'PASS: {name} START gameplay, 600 frames, no illegal opcodes')

    # Walk and repeatedly jump across the Stallar level, including camera
    # scrolling. NEMU and GEM consume the same per-frame controller recording.
    replay = work / 'scroll.input'
    recording = bytearray(b'NESINP01')
    for frame in range(1200):
        buttons = 8 if 120 <= frame < 180 else 0
        if 300 <= frame < 1000:
            buttons = 128 | (1 if frame % 45 < 20 else 0)
        recording.extend((buttons, 0))
    replay.write_bytes(recording)
    rom = ROOT / 'roms/stallar.nes'
    for frames in (240, 360, 480, 600, 1200):
        golden, image = work / 'golden.ppm', work / 'raster.ppm'
        env = dict(os.environ)
        if frames in (600, 1200): env['NEMU_NES_TRACE'] = str(nt)
        run([NEMU, '--headless', '--no-audio', f'--frames={frames}',
             f'--replay-input={replay}', f'--screenshot={golden}', rom], env=env)
        args = [GEM, 'game', rom, frames, image, f'--replay-input={replay}']
        if frames in (600, 1200): args.append(f'--trace-state={gt}')
        output = run(args)
        check_game(output, frames)
        assert image.read_bytes() == golden.read_bytes(), f'raster mismatch at frame {frames}'
        rows = compare(nt, gt) if frames in (600, 1200) else 0
        print(f'PASS: Stallar move/jump replay {frames} frames, framebuffer matches NEMU'
              + (f', {rows} CPU/cycle/PPU rows match' if rows else ''))
