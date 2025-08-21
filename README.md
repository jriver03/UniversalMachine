# UM Emulator (C)

**Course warmup:** Implement an emulator for the "Universal Machine" architecture defined in 'machine-specification.pdf'.
**Goals:** (1) practice bit-level computer organization, (2) refresh low-level C, (3) exercise careful spec reading.

---

## Build

The emulator is a single C17 program at 'src/loader.c'. Two typical builds are shown below

### Release (for timing on callisto )
'''bash
cc -std=c17 -O3 -DNDEBUG -Wall -Wextra -o loader src/loader.c

### Debug (optional for local correctness and sanitizer checks)
'''bash
cc -std=c17 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wshadow -o loader src/loader.c

## How to Run

From repo root after building loader:
'''bash
# Hello world
./loader programs/helloworld.um

# Square (reads one decimal line from stdin, prints it square)
echo 12 | ./loader programs/square.um

# Waste (infinite allocate loop) - stop with Ctrl-C
./loader programs/waste.um

# Sandmark (performance benchmark)
./loader programs/sandmark.um

## Timing Result

Host: callisto
Command used:  /usr/bin/time -p ./loader programs/sandmark.um
Result: real: 12.05 s
Time on stopwatch: 12.25s

## Design Notes

**Program load (array 0)**: The .um file is read as big-endian 32-bit words, that word array then becomes array 0 (the program). All 8 registers initialize to 0, and pc = 0.

**Fetch/Execute Loop:** Each cycle:
    1. Check pc is in bounds of array 0 (fail if not).
    2. Fetch w = array0[pc], decode op in bits 28..31.
    3. Execute the operation, advance pc by 1, except for opcode 12 which sets pc = C (no incrementation).

**Fielding:**
    1. Standard ABC layout: A = bits 6..8, B = 3..5, C = 0.2.
    2. Opcode 13 (Load-Immediate) uses a special layout: A in bits 25..27, 25-bit immediate in 0.24 -> regs[A] = imm.

**Arrays/Heap Model:**
    1. (8) **Allocation:** creates a zero-initilized array of length regs[C], then stores a non-zero id into regs[B].
    2. (9) **Deallocation:** free array id regs[C] (must be active and not 0), ids are reused.
    3. (1) **Index:** regs[A] = mem[regs[B]][regs[C]] with **acitve+bounds** checks.
    4. (2) **Update:** mem[regs[A]]][regs[B]] = regs[C] with **active+bounds** checks.

**ALU/ Logic/ I/O:**
    1. (3) **Add**, (4) **Multiply**, (5) **Unsigned Division (fails on /0)**, (6) **Nand**.
    2. (10) Output: regs[C] must be 0..255 (print as a byte, else fails).
    3. (11) Input: read one byte, on EOF write 0xFFFFFFFF to regs[C].

**Control:**
    1. (0) **Conditional Move:** (if regs[C] != 0, then regs[A] = regs[B])
    2. (7) **Halt**: clean shutdown.
    3. (12) **Load Program:** if regs[B] != 0, **deep copy** that array into array 0, then set pc = regs[C] (no increment).

**Exceptions/Fail Conditions:**
    1. PC out-of-bounds at cycle start.
    2. Index/update on **inactive** array or **out-of-bounds** offset.
    3. Deallocate id 0 or an inactive id.
    4. Divide by zero (opcode 5).
    5. Output value > 255 (opcode 10).
    6. Load Program from an inactive id (opcode 12)

## Files Included
    1. src/loader.c - emulator implementation (C17)
    2. programs/helloworld.um
        programs/square.um
        programs/waste.um
        programs/sandmark.um
    3. machine-specification.pdf - architecture & ISA
    4. README.md - this document
    5. Makefile - convenience targets (debug/release).


