/*********************************/
/* README                        */
/* CS554 Julian Fong             */
/* Simple machine emulator       */
/*********************************/

usage:
    ./machine tests/sandmark.um

disassembler:
    ./dasm disasembled/square.um > square_dasm.uma

assembler:
    python assembler.py disasembled/square.uma square_asm.um

can be made from source using command `make`

the fastest version of the machine can be made using `make fast`

To test the assembler and disassembler run `make test`

Git commits that improved my speed:

I haven't commited it yet but my final speedup was from the gotos instead 
of large opcode switch. 

6d5f9e4 (HEAD -> jumps, main) Made file load faster:
    I used fread instead of a nested byte swaping loop so the compiler
    could optimize the loop. 

9fd6fb0 Got speedup making array struct smaller:
    I stopped tracking the array ID in the array struct so the cache 
    utiliation could make better use of the small structs. I also 
    stopped tracking the ID stack as alinked list and instead used something similar
    to my array allocation table.

07fb122 Removed a lot of if statements and saw speedup:
    Tried to avoid prefetch misses and branch misprediction

482b6e7 Took a second off removing error messages:
    I just stopped checking for errors and assumed the program would 
    run without breaking the rules. 

SANDmark results:
jfong2702@b146-46:~/Classes/CS554/warmups/exercise_1$ date
Sun Sep  7 08:16:56 PM MDT 2025
jfong2702@b146-46:~/Classes/CS554/warmups/exercise_1$ time ./machine tests/sandmark.um
trying to Allocate array of size 0..
trying to Abandon size 0 allocation..
trying to Allocate size 11..
trying Array Index on allocated array..
trying Amendment of allocated array..
checking Amendment of allocated array..
trying Alloc(a,a) and amending it..
comparing multiple allocations..
pointer arithmetic..
check old allocation..
simple tests ok!
about to load program from some allocated array..
success.
verifying that the array and its copy are the same...
success.
testing aliasing..
success.
free after loadprog..
success.
loadprog ok.
 == SANDmark 19106 beginning stress test / benchmark.. ==
100. 12345678.09abcdef
99.  6d58165c.2948d58d
98.  0f63b9ed.1d9c4076
97.  8dba0fc0.64af8685
96.  583e02ae.490775c0
95.  0353a77b.2f02685c
94.  aa25a8d7.51cb07e5
93.  e13149f5.53a9ae5d
92.  abbbd460.86cf279c
91.  2c25e8d8.a71883a9
90.  dccf7b71.475e0715
89.  49b398a7.f293a13d
88.  9116f443.2d29be37
87.  5c79ba31.71e7e592
86.  19537c73.0797380a
85.  f46a7339.fe37b85a
84.  99c71532.729e2864
83.  f3455289.b84ced3d
82.  c90c81a9.b66fcd61
81.  087e9eef.fc1c13a6
80.  e933e2f5.3567082f
79.  25af849e.16290d7b
78.  57af9504.c76e7ded
77.  68cf6c69.6055d00c
76.  8e920fbd.02369722
75.  eb06e2de.03c46fda
74.  f9c40240.f1290b2a
73.  7f484f97.bc15610b
72.  1dabb00e.61e7b75b
71.  dceb40f5.207a75ca
70.  c3ed44f5.db631e81
69.  b7addb67.90460bf5
68.  ae710a90.04b433ef
67.  9ca2d5f0.05d3b631
66.  4f38abe0.4287cc05
65.  10d8691d.a5c934f8
64.  27c68255.52881eaa
63.  a0695283.110266b7
62.  336aa5dd.57287a9b
61.  b04fe494.d741ddbd
60.  2baf3654.9e33305a
59.  fd82095d.683efb19
58.  d0bac37f.badff9d7
57.  3be33fcc.d76b127e
56.  7f964f18.8b118ee1
55.  37aeddc8.26a8f840
54.  d71d55ff.6994c78f
53.  bf175396.f960cc54
52.  f6c9d8e1.44b81fd5
51.  6a9b4d86.fe7c66cb
50.  06bceb64.d5106aad
49.  237183b6.49c15b01
48.  4ec10756.6936136f
47.  9d1855a7.1e929fe8
46.  a641ede3.36bff422
45.  7bbf5ad4.dd129538
44.  732b385e.39fadce7
43.  b7f50285.e7f54c39
42.  42e3754c.da741dc1
41.  5dc42265.928ea0bb
40.  623fb352.3f25bc5b
39.  491f33d9.409bca87
38.  f0943bc7.89f512be
37.  80cdbc9d.8ad93517
36.  c1a8da99.32d37f3f
35.  91a0b15c.6df2cf4e
34.  50cf7a7a.f0466dc8
33.  02df4c13.14eb615d
32.  2963bf25.d9f06dfe
31.  c493d2db.f39ce804
30.  3b6e5a8e.5cf63bd7
29.  4c5c2fbe.8d881c00
28.  9b7354a6.81181438
27.  ae0fe8c6.ec436274
26.  e786b98d.f5a4111d
25.  a7719df1.d989d0b6
24.  beb9ebc0.6c56750d
23.  edf41fcb.e4cba003
22.  97268c46.713025f1
21.  deb087db.1349eb6a
20.  fc5221f0.3b4241bf
19.  3fa4370d.8fa16752
18.  044af7de.87b44b11
17.  2e86e437.c4cdbc54
16.  fd7cd8aa.63b6ca23
15.  631ceaad.e093a9d5
14.  01ca9732.52962532
13.  86d8bcf5.45bdf474
12.  8d07855b.0224e80f
11.  0f9d2bee.94d86c38
10.  5e6a685d.26597494
9.   24825ea1.72008775
8.   73f9c0b5.1480e7a3
7.   a30735ec.a49b5dad
6.   a7b6666b.509e5338
5.   d0e8236e.8b0e9826
4.   4d20f3ac.a25d05a8
3.   7c7394b2.476c1ee5
2.   f3a52453.19cc755d
1.   2c80b43d.5646302f
0.   a8d1619e.5540e6cf
SANDmark complete.

real	0m12.260s
user	0m12.236s
sys	0m0.032s

AI Aknowledgement

AI Model: ChatGPT 5 Instant&Thinking ~free version

Prompt: what's the fastest and best optimization flags I should use? 

Response: For your makefile, you’ll want to move away from -g (debug builds) 
for speed and instead add compiler optimizations. Here’s a good setup:

Recommended optimization flags
	•	-O3 → Maximum compiler optimization for speed. Aggressive loop unrolling, inlining, vectorization, etc.
	•	-march=native → Generates code optimized for your specific CPU (uses all CPU instruction sets available on the build machine).
	•	-flto → Link Time Optimization, lets the compiler optimize across files.
	•	-fomit-frame-pointer → Removes frame pointer usage on platforms where it isn’t needed, freeing a register.
	•	-funroll-loops → Explicitly unrolls loops, often useful in tight interpreters.

Example CFLAGS

CFLAGS = -O3 -march=native -flto -fomit-frame-pointer -funroll-loops

Updated makefile rules

SRC=machine.c
HEADER=machine.h
CFLAGS=-O3 -march=native -flto -fomit-frame-pointer -funroll-loops

.PHONY: clean all

all: machine dasm

machine: $(SRC) $(HEADER)
	gcc $(CFLAGS) $(SRC) -o machine -DDEBUG=0

dasm: $(SRC) $(HEADER)
	gcc $(CFLAGS) $(SRC) -o dasm -DDEBUG=1

clean:
	rm -f machine dasm

👉 This will give you a release build that’s fast while still letting you build a debug/disassembler version with tracing.

Do you also want me to add a separate debug build target (with -O0 -g) so you can easily switch between performance and debugging?


prompt: How can I make my swich expression faster with gotos and jumps in C? 

reply:
In C you can replace a big switch (or manual goto) with a jump table using
GCC/Clang’s labels-as-values extension (&&label and goto *ptr).

Here’s a minimal example:

#include <stdio.h>

int main(void) {
    int pc = 0;
    int program[] = {0, 1, 2, 99};  // opcodes: 0=printA, 1=printB, 2=inc, 99=halt

    static void* jump_table[] = { &&op0, &&op1, &&op2, &&halt };

    for (;;) {
        int opcode = program[pc++];
        if (opcode == 99) goto *jump_table[3];   // halt
        if (opcode < 0 || opcode > 2) { printf("bad opcode\n"); break; }
        goto *jump_table[opcode];  // dispatch

op0:    printf("A\n"); goto next;
op1:    printf("B\n"); goto next;
op2:    printf("inc!\n"); goto next;

next:   continue;
halt:   printf("halt\n"); break;
    }
}

🔑 Key points:
	•	&&label gives you a pointer to a label.
	•	Store those in a table (jump_table).
	•	Use goto *jump_table[idx]; to jump directly.

This avoids branch chains and can be much faster than a giant switch for interpreters.
