# UM Emulator (C)

**Course warmup:** an emulator for the “Universal Machine” architecture from `machine-specification.pdf`.  
**Goals:** (1) bit-level computer organization, (2) low-level C practice, (3) spec‑driven implementation.

---

## Quick Start

```bash
# Build (debug by default)
make

# Hello world
./BUILD/loader programs/helloworld.um

# Square (reads one decimal line from stdin, prints its square)
echo 12 | ./BUILD/loader programs/square.um
```

---

## Build

There’s a `Makefile` with three modes and two tools:

```bash
# Debug build (O0, ASan/UBSan, warnings)
make               # same as: make debug

# Release build (O3, -DNDEBUG)
make release

# Performance build (O3, -DNDEBUG, -flto)
make perf

# Tools (disassembler & assembler)
make disasm asm

# Clean
make clean
```

> Binaries are written to `BUILD/`:  
> - `BUILD/loader`, `BUILD/loader-release`, `BUILD/loader-perf`  
> - `BUILD/disasm`, `BUILD/asm`

---

## Usage

```
UM emulator

Usage:
  ./BUILD/loader [--trace] <program.um>

Options:
  -h, --help   Show help and exit
  --trace      Print a per-instruction trace to stderr

Environment (tracing):
  UM_TRACE_LIMIT=N   Stop printing trace once PC >= N
```

**Examples**

```bash
# Just run a program
./BUILD/loader programs/helloworld.um

# Trace the first 10 instructions (stderr), while stdout still prints program output
UM_TRACE_LIMIT=10 ./BUILD/loader --trace programs/helloworld.um 2> traces/helloworld.trace
```

---

## What’s Implemented

- **ISA opcodes (0..13):** `cmov, aidx, aupd, add, mul, div, nand, halt, alloc, dealloc, out, in, loadprog, loadimm`
- **Registers:** 8 × 32-bit, all initialized to 0
- **Memory model:** “arrays” with ID 0 holding the program; non-zero arrays are heap-allocated and IDs are recycled via a free-ID LIFO stack
- **Endian:** `.um` files are **big‑endian** 32-bit words
- **Fielding:** standard ABC layout (A:6..8, B:3..5, C:0..2); `loadimm` (13) uses A:25..27 and immediate:0..24

---

## Workflow

### Program Load (array 0)
- The `.um` file is read as big‑endian 32‑bit words into a heap buffer.
- That buffer becomes **array 0**.
- PC starts at 0; all registers are 0.

### Fetch/Execute Loop
1. **Bounds check**: if `pc >= len(array0)`, fail.
2. Fetch `w = array0[pc]`.
3. Decode `op = w >> 28`.
4. Execute:
   - Most ops: `pc++` after executing.
   - **Opcode 13 (`loadimm`)**: special fielding; still `pc++`.
   - **Opcode 12 (`loadprog`)**: if `B != 0`, deep‑copy `mem[B]` into array 0; then `pc = regs[C]` (**no increment**).

### Heap / Arrays
- **alloc (8)**: allocate a zero‑initialized array of length `regs[C]`, store **non‑zero** id into `regs[B]`.
- **dealloc (9)**: free array `regs[C]` (must be active and not 0), push id on the free‑ID stack.
- **aidx (1)**: `regs[A] = mem[regs[B]][regs[C]]` with active + bounds checks.
- **aupd (2)**: `mem[regs[A]][regs[B]] = regs[C]` with active + bounds checks.

### ALU, I/O
- **add (3), mul (4):** full 32‑bit wrap semantics.
- **div (5):** unsigned; divide by 0 -> fail.
- **nand (6):** bitwise `~(B & C)`.
- **out (10):** `0..255` printed as a byte; anything else -> fail.
- **in (11):** one byte; on EOF store `0xFFFFFFFF`.

### Control
- **cmov (0):** if `C != 0` then `A = B`.
- **halt (7):** clean shutdown (frees arrays).
- **loadprog (12):** deep copy into array 0 if `B != 0`, then `pc = C` (no `pc++`).

---

## Disassembler & Assembler

Both are included (simple and single file tools).

### Disassembler

```bash
make disasm
./BUILD/disasm programs/helloworld.um > out/disasm_helloworld.txt
./BUILD/disasm programs/square.um > out/disasm_square.txt
```

Output format is intentionally loose but shows `[pc]` and decoded ops.

### Assembler

```bash
make asm
# Assemble a .uma file to .um (big-endian words to stdout)
./BUILD/asm programs/helloworld.uma -o out/helloworld_from_asm.um
./BUILD/asm programs/square.um -o out/square_from_asm.um
```

The assembly syntax follows the examples used in class (labels optional, immediate forms allowed for `loadimm`, register forms for others). See `programs/*.uma` for reference.

---

## Traces

Traces print to **stderr** when `--trace` is used. Redirect stderr to collect:

```bash
mkdir -p traces

# Hello world
./BUILD/loader --trace programs/helloworld.um \
  2> traces/helloworld.trace > traces/helloworld.out

# Square (n=12)
printf "12\n" | ./BUILD/loader --trace programs/square.um \
  2> traces/square_12.trace > traces/square_12.out
```

---

## Timing (Sandmark)

**Host:** callisto  
**Command:** `/usr/bin/time -p ./BUILD/loader programs/sandmark.um`  

**Example result:**
```
real 15.12
user 15.09
sys  0.00
```

> For best numbers, use the perf build:
> ```bash
> make perf
> /usr/bin/time -p ./BUILD/loader-perf programs/sandmark.um
> ```

---

## Proofs

See proof-b146-46.txt

---

## File Layout

```
.
├─ src/
│  ├─ loader.c        # emulator
│  ├─ disasm.c        # disassembler (optional tool)
│  └─ asm.c           # assembler   (optional tool)
├─ programs/
│  ├─ helloworld.um
│  ├─ square.um
│  ├─ waste.um
│  ├─ sandmark.um
│  └─ *.uma (if used)
├─ traces/
├─ out/
├─ machine-specification.pdf
├─ Makefile
└─ README.md
```

---

## Known Limitations

- Emulator expects valid `.um` files (proper 4‑byte alignment and big‑endian words).
- No CLI for loading multiple programs; use loader once per run.
- Minimal error messages (by design, to keep the core small).

---

## Warm up #3: 09/07/2025

Attempted performance improvements that reduced Sandmark wall-time on b146-46 from 16.7s -> 14.2s (~15%) and on my Mac from 22.3s -> 19.6s (~12%). I removed all trace/diagnostic overhead from the hot path (trace now compiled out unless -DTRACE and only active when --trace is passed), added a perf build (-O3 -DNDEBUG -flto), and cached the program/length for faster fetch and bounds checks (refreshing it on loadprog). The remaining VM semantics are unchanged. 

**1. Helpful Changes (kept)**

	- **Trace fully gated**: all TRACEF(...) compiled out in perf/release; runtime tracing only when trace is passed in debug builds. This eliminated format/IO and dead branches from the hot loop.
	- **Perf toolchain flags:** -O3 -DNDEBUG -flto (link-time inlining & de-virt across TU boundaries). This allowsy the compiler inline helpers (bitfield extractors, be32 load) and tighten the loop.
	= **Fetch/bounds micro-cache:** code0 = g_arr[0].data; code0_len = g_arr[0].len; reused each cycle; refresh after loadprogt. This avoids two global/struct loads per iteration and helps the branch predictor on the loop guard.
	- **Tidy hot helpers:** inlined bitfield extractors; kept 32-bit types for registers/memory to match ISA and avoid 64 -> 32 truncations.

**2. Changes that Failed**

	- '-march=native' and '-fomit-frame-pointer' resulted in no consistent speedup on lab Linux; noise-level difference on macOS. Left disabled to keep binaries broadly portable.
	- Indirect dispatch (function-pointer table) resulted in a slower (~1-3%) vs switch due to harder branch prediction/BTB for the indirect call on this tight ISA.
	- Computed gotos (GCC/Clang extension) that tied to the last failed improvement; slightly worse than switch on our tests. Not worth the probablility hit.
	- Pre-decoding the whole program (struct of {op, A, B, C, imm}) that added ~5-8% regression on Sandmark, with th extra memory traffic and i-cache pressure outweighing the save three bitfield extracts. 
	- mmap loading vs fread resulted with no runtime difference (load cost is amortized; the benchmark is CPU-bound in the interpreter, not IO-bound).

---

## Acknowledgments

	•	Assignment spec & sample programs:
	  •	Course handout: machine-specification.pdf (provided by the instructor).
	  •	Sample UM binaries & assembly: programs/helloworld.um, programs/square.um, programs/sandmark.um, programs/waste.um, and corresponding .uma files (provided by the course).

	•	C language & standard library:
	  •	C17 reference (functions, headers):
	    •	fopen, fread, fwrite, malloc, calloc, realloc, free, memcpy, memset, strtoul, strtol: https://en.cppreference.com/w/c
	  •	Integer types & conversions: https://en.cppreference.com/w/c/language/types

	•	POSIX I/O & large-file support:
	  •	fseeko, ftello, _FILE_OFFSET_BITS=64: https://man7.org/linux/man-pages/man3/fseeko.3.html

	•	Compiler & sanitizers (Clang/LLVM):
	  •	AddressSanitizer (ASan): https://clang.llvm.org/docs/AddressSanitizer.html
	  •	UndefinedBehaviorSanitizer (UBSan): https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
	  •	Link Time Optimization (LTO): https://clang.llvm.org/docs/LinkTimeOptimization.html

	•	Build system:
	  •	GNU Make manual: https://www.gnu.org/software/make/manual/make.html
	
  •	Version control:
	  •	Git book (everyday commands & workflows): https://git-scm.com/book/en/v2
	  •	Git reference (push/pull, remotes): https://git-scm.com/docs

	•	Shell usage (Bash/Zsh):
	  •	Redirections & pipelines (Bash Reference Manual §3.7): https://www.gnu.org/software/bash/manual/bash.html#Redirections
	  •	Process substitution (useful for diff -u <(…) …): https://www.gnu.org/software/bash/manual/bash.html#Process-Substitution

	•	Timing:
	  •	GNU time utility (/usr/bin/time -p): https://www.gnu.org/software/time/

	•	Acknowledgment of tooling
	  •	ChatGPT (GPT-5 Thinking) was used for planning, polish, and small test/Makefile scaffolding guidance.

---