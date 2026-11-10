# Microsoft BASIC 1.1 for 6502EMU

This directory contains a build of **Microsoft BASIC 1.1** for the 6502EMU
machine.  The interpreter is the KIM-1 configuration from Michael Steil's
[msbasic](https://github.com/mist64/msbasic) project (upstream commit
`2a0bc2f`), with a small console glue patch.

## Entry point and memory

- BASIC code loads at `$2000`
- Cold start: `$4058`
- Program RAM begins after the interpreter (about `$425C`)

The bundled `msbasic.bin` is already built, so the demo can run without cc65.
To rebuild it:

```sh
./basic/build.sh
```

## Console glue

The KIM monitor calls were replaced with 6502EMU host I/O:

- `MONRDKEY`: reads a byte from `$F003`
- `MONCOUT`: writes a byte to `$F001`
- `ISCNTC`: returns immediately (Ctrl-C is handled by the host terminal)

The emulator translates LF/CR to CR for guest input.  A host EOF stops the
simulation cleanly.

## Run

Interactive:

```sh
./basic/run.sh
```

At the boot prompts enter a memory top and terminal width, for example:

```text
MEMORY SIZE? 32768
TERMINAL WIDTH? 80
WANT SIN-COS-TAN-ATN? N
```

Then type BASIC lines (uppercase; Microsoft BASIC 1.1 is case-sensitive on
this target):

```basic
10 PRINT "HELLO FROM BASIC"
20 INPUT "COUNT"; N
30 FOR I=1 TO N
40 PRINT I*I
50 NEXT I
60 END
RUN
5
```

Press Ctrl-D to stop the emulator.

Non-interactive demo:

```sh
./basic/run-demo.sh
```

## License

The upstream msbasic project states that it is distributed under the
2-clause BSD license.  The source in `msbasic/` is unmodified except for the
console glue described above.

Copyright (c) Michael Steil and contributors.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
