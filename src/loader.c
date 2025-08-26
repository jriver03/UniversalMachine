// UM emulator
// build (debug): cc -std=c17 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -o loader src/loader.c
// build (release): cc -std=c17 -O3 -DNDEBUG -Wall -Wextra -o loader src/loader.c

/* Mac -> Linux stuff */
#define _POSIX_C_SOURCE 200809L // expose POSIX_APIs like fseeko/ftello
#define _FILE_OFFSET_BITS 64 // make off_t 64-bit
#include <sys/types.h> // declares off_t

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

/*-------------- tiny utils --------------- */ 
static void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static const char *build_mode(void) {
    #if defined(NDEBUG)
        return "release/perf (-O3, -DNDEBUG; perf may also add -flto)";
    #else
        return "debug (-O0, ASan/UBSan)";
    #endif
}

static void print_help(const char *prog) {
    fprintf(stdout, 
    "UM emulator\n"
    "\n"
    "Usage:\n"
    "  %s [--trace] <program.um>\n"
    "\n"
    "Options:\n"
    "  -h, --help  Show this help and exit\n"
    "  --trace     Print a per-instruction trace to stderr\n"
    "\n"
    "Environment (tracing):\n"
    "  UM_TRACE_LIMIT=N  Stop printing trace once PC >= N\n"
    "\nBuild modes (see Makefile targets):\n"
    "  debug   -O0 + ASan/UBSan\n"
    "  release -O3 -DNDEBUG\n"
    "  perf    -O3 -DNDEBUG -flto\n"
    "\n"
    "\nThis binary was built as: %s\n", 
    prog, build_mode());
}

/* assemble a big-endian 32-bit word from 4 bytes (A is MSB) */
static inline uint32_t be32_from(const unsigned char b[4]) {
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) |
           ((uint32_t)b[3] << 0);
}

/* bitfield helpers */
static inline unsigned OPC(uint32_t w) { return w >> 28; } // bits 28..31
static inline unsigned ABC_A(uint32_t w) { return (w >> 6) & 7u; } // bits 6..8
static inline unsigned ABC_B(uint32_t w) {return (w >> 3) & 7u; } // bits 3..5
static inline unsigned ABC_C(uint32_t w) { return (w >> 0) & 7u; } // bits 0..2
static inline unsigned LI_A(uint32_t w) { return (w >> 25) & 7u; } // bits 25..27
static inline unsigned LI_VAL(uint32_t w) {return w & 0x1FFFFFFu; } // bits 0..24

static const char *opname(unsigned op) {
    switch (op) {
        case 0: return "cmov";
        case 1: return "aidx";
        case 2: return "aupd";
        case 3: return "add";
        case 4: return "mul";
        case 5: return "div";
        case 6: return "nand";
        case 7: return "halt";
        case 8: return "alloc";
        case 9: return "dealloc";
        case 10: return "out";
        case 11: return "in";
        case 12: return "loadprog";
        case 13: return "loadimm";
        default: return "?";
    }
}

/* ------------------- array registry ("heap") ---------------- */
typedef struct {
    uint32_t *data;
    size_t len;
    int active; // 1 if allocated (including id 0 for program), 0 otherwise
} UMArray;

// Registry
static UMArray *g_arr = NULL; // ids: 0 .. g_arr_len - 1
static size_t g_arr_len = 0;
static size_t g_arr_cap = 0;

// free-id stack
static uint32_t *g_free_ids = NULL; // LIFO stack of reusable ids
static size_t g_free_len = 0;
static size_t g_free_cap = 0;

/* --------------- tiny helpers ------------------ */
static void arr_reserve(size_t need_cap) {
    if (g_arr_cap >= need_cap) return;
    
    size_t nc = g_arr_cap ? g_arr_cap : 4;
    
    while (nc < need_cap) nc <<= 1;

    UMArray *na = (UMArray*)realloc(g_arr, nc * sizeof(UMArray));

    if (!na) die("out of memory (arr)");

    // zero new slots so .active defaults to 0
    memset(na + g_arr_cap, 0, (nc - g_arr_cap) * sizeof(UMArray));
    g_arr = na;
    g_arr_cap = nc;
}

static void freeids_reserve(size_t need_cap) {
    if (g_free_cap >= need_cap) return;

    size_t nc = g_free_cap ? g_free_cap : 8;

    while (nc < need_cap) nc <<= 1;

    uint32_t *nf = (uint32_t*)realloc(g_free_ids, nc * sizeof(uint32_t));

    if (!nf) die("out of memory (free_ids)");

    g_free_ids = nf;
    g_free_cap = nc;
}
static uint32_t id_acquire(void) {
    if (g_free_len > 0) return g_free_ids[--g_free_len];
    arr_reserve(g_arr_len + 1);
    return (uint32_t)g_arr_len++; // after boot, this will be >= 1
}

static void id_release(uint32_t id) {
    freeids_reserve(g_free_len + 1);
    g_free_ids[g_free_len++] = id;
}

static void arrays_boot(uint32_t *program, size_t nwords) {
    arr_reserve(1);
    g_arr_len = 1; // id 0 exists
    g_arr[0].data = program; // array 0 holds the program
    g_arr[0].len = nwords;
    g_arr[0].active = 1;
}

static void arrays_destroy(void) {
    for (size_t i = 0; i < g_arr_len; ++i) {
        free(g_arr[i].data); // free(NULL) ok, frees program aswell
        g_arr[i].data = NULL;
        g_arr[i].len = 0;
        g_arr[i].active = 0;
    }

    free(g_arr);
    g_arr = NULL;
    g_arr_len = g_arr_cap = 0;

    free(g_free_ids);
    g_free_ids = NULL;
    g_free_len = g_free_cap = 0;
}

static void fail_and_exit(const char *msg) {
    fprintf(stderr, "fail: %s\n", msg);
    arrays_destroy();
    exit(1);
}

static void dump_reg_changes(uint32_t before[8], uint32_t after[8]) {
    for (int i = 0; i < 8; ++i) {
        if(before[i] != after[i]) {
            fprintf(stderr, "   r%d: %u -> %u\n", i, before[i], after[i]);
        }
    }
}

/* ----------------- main ----------------------*/
int main(int argc, char **argv) {
    int trace = 0;
    int argi = 1;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }
    }

    if (argc >= 2 && strcmp(argv[argi], "--trace")==0) {
        trace = 1;
        ++argi;
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    unsigned trace_limit = 0;

    if (trace) {
        const char *lim = getenv("UM_TRACE_LIMIT");
        if (lim && *lim) {
            trace_limit = (unsigned)strtoul(lim, NULL, 0);
        }
    }

    if (argc - argi != 1) {
        fprintf(stderr, "usage: %s [--trace] <program.um>\n"
                        "try '%s --help' for more info\n", argv[0], argv[0]);
        return 2;
    }
    const char *path = argv[argi];

    FILE *fPath = fopen(path, "rb");
    if (!fPath) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return 1;
    }

    /* Find file size (64-bit friendly), then rewind */
    if (fseeko(fPath, 0, SEEK_END) != 0) {
        fclose(fPath);
        die("fseeko failed");
    }

    off_t size = ftello(fPath);

    if (size < 0) {
        fclose(fPath);
        die("ftello failed");
    }

    if (fseeko(fPath, 0, SEEK_SET) != 0) {
        fclose(fPath);
        die("fseeko rewind failed");
    }

    /* Program file size is always divisible by 4. */
    if (size == 0) {
        fclose(fPath);
        die(".um file is empty");
    }

    if ((size & 3) != 0) {
        fclose(fPath);
        die(".um size not divisible by 4");
    }

    size_t nwords = (size_t)(size / 4);
    uint32_t *words = (uint32_t*)malloc(nwords * sizeof(uint32_t));

    if (!words) {
        fclose(fPath);
        die("out of memory");
    }

    /* Read 4 bytes -> assemble one big-endian word -> store */
    unsigned char buf[4];
    for (size_t i = 0; i < nwords; ++i) {
        size_t got = fread(buf, 1, 4, fPath);
        if (got != 4) {
            free(words);
            fclose(fPath);
            die("short read");
        }
        words[i] = be32_from(buf); /* A is MSB */
    }
    fclose(fPath);

    // boot machine arrays: id 0 = program
    arrays_boot(words, nwords);

    uint32_t regs[8] = {0}; // 8 general-purpose registers
    uint32_t pc = 0; // Program counter starts at 0

    /* --------------------- fetch / decode / execute loop -------------------*/
    for (;;) {
        if (trace && trace_limit && pc >= trace_limit) {
            fprintf(stderr, "[trace disabled after pc=%u]\n", pc);
            trace = 0;
        }
        // Exception: if at cycle start PC outside 0-array capacity is a Fail
        if (pc >= g_arr[0].len) {
            fail_and_exit("PC out of bounds at cycle start");
        }

        uint32_t w = g_arr[0].data[pc];
        unsigned op =  OPC(w);
        
        uint32_t before[8];
        if (trace) {
            memcpy(before, regs, sizeof before);
        }

        if (trace) {
            if (op == 13u) {
                fprintf(stderr, "[pc=%u] 0x%08x %-8s A=%u imm=%u\n",
                pc, w, opname(op), LI_A(w), LI_VAL(w));
            } else {
                fprintf(stderr, "[pc=%u] 0x%08x %-8s A=%u B=%u C=%u | rA=%u rB=%u rC=%u\n",
                pc, w, opname(op), ABC_A(w), ABC_B(w), ABC_C(w), (unsigned)regs[ABC_A(w)],
                (unsigned)regs[ABC_B(w)], (unsigned)regs[ABC_C(w)]);
            }
        }

        // 13. Load Immediate: uses special fields
        if (op == 13u) {
            unsigned A = LI_A(w);
            uint32_t imm25 = LI_VAL(w); // bits 0..24
            regs[A] = imm25;
            pc++;
        } else {

            // standard layout (A=6..8, B=3..5, C=0..2)
            unsigned A = ABC_A(w), B = ABC_B(w), C = ABC_C(w);

            switch (op) {

                /* 0: Conditional Move: if C != 0 then A <- B */
                case 0: {
                    if (regs[C] != 0) regs[A] = regs[B];
                    pc++;
                    break;
                }

                /* 1: Array Index: A <- mem[B][C] (bounds + active checks) */
                case 1: {
                    uint32_t id = regs[B], off = regs[C];

                    if (id >= g_arr_len || !g_arr[id].active) fail_and_exit("index: inactive array");

                    if ((size_t)off >= g_arr[id].len) fail_and_exit("index: offset OOB");
                
                    regs[A] = g_arr[id].data[off];
                    pc++;
                    break;
                }

                /* 2: Array Update: mem[A][B] <- C (bounds + active checks) */
                case 2: {
                    uint32_t id = regs[A], off = regs[B], val = regs[C];

                    if  (id >= g_arr_len || !g_arr[id].active) fail_and_exit("update: inactive array");

                    if ((size_t) off >= g_arr[id].len) fail_and_exit("update: offset OOB");

                    g_arr[id].data[off] = val;
                    pc++;
                    break;     
                }
            
                /* 3: Addition: A <-  B + C (mod 2^32) */
                case 3: {
                    regs[A] = regs[B] + regs[C]; // uint32_t wraps mod 2^32
                    pc++;
                    break;
                }

                /* 4: Multiplication: A <- B * C (mod 2^32) */
                case 4: {
                    regs[A] = regs[B] * regs[C];
                    pc++;
                    break;
                }

                /* 5: Division (unsigned): A <- B / C, /0 = Fail */
                case 5: {
                    uint32_t denom = regs[C];
                    if (denom == 0) { // Divde by 0 is a fail
                        fail_and_exit("divide by zero");
                    }
                    regs[A] = regs[B] / denom; // unsigned division
                    pc++;
                    break;
                }

                /* 6: Not-And: A <- ~(B & C) */
                case 6: {
                    regs[A] = ~(regs[B] & regs[C]);
                    pc++;
                    break;
                }

                /* 7: Halt*/
                case 7: {
                    arrays_destroy();
                    return 0; 
                }

                /* 8: Allocation: B gets new nonzero id for zeroed array(C) */
                case 8: {
                    uint32_t n = regs[C];
                    uint32_t *data = NULL;
                
                    if (n > 0) {
                        data = (uint32_t*)calloc((size_t)n, sizeof(uint32_t)); // zero-init
                        if (!data) fail_and_exit("alloc: OOM");
                    }

                    uint32_t id = id_acquire();

                    if (id == 0) fail_and_exit("alloc: id 0 reserved");
                    if (trace) fprintf(stderr, "    alloc -> id=%u, len=%u\n", id, (unsigned)n);

                    g_arr[id].data = data;
                    g_arr[id].len = n;
                    g_arr[id].active = 1;
                    regs[B] = id;
                
                    pc++;
                    break;
                }

                /* 9: Abandonment: deallocate array id = C (not 0, must be active) */
                case 9: {
                    uint32_t id = regs[C];

                    if (id == 0 || id >= g_arr_len || !g_arr[id].active) {
                        fail_and_exit("dealloc: invalid or inactive id");
                    }

                    if (trace) fprintf(stderr, "    dealloc id=%u\n", id);

                    free(g_arr[id].data);
                
                    g_arr[id].data = NULL;
                    g_arr[id].len = 0;
                    g_arr[id].active = 0;

                    id_release(id);
                    pc++;
                    break;
                }

                /* 10: Output: print byte in C (0..255), else Fail */
                case 10: {
                    uint32_t v = regs[C];

                    if (v > 255u) { // Output must be 0..255
                        fail_and_exit("output: value > 255");
                    }

                    putchar((int)(v & 0xFF));
                    fflush(stdout);
                    pc++;
                    break;
                }

                /* 11: Input: read one byte into C, EOF -> 0xFFFFFFFF */
                case 11: {
                    int ch = getchar();
                    if (ch == EOF) { 
                        regs[C] = 0xFFFFFFFFu;
                    } else {
                        regs[C] = (uint32_t) (unsigned char) ch;
                    }
                    pc++;
                    break;
                }

                /* 12: Load Program: if B != 0, duplicate mem[B] into mem[0], pc=C (no pc++) */
                case 12: {
                    uint32_t id = regs[B];
                    uint32_t new_pc = regs[C];

                    if (id != 0) {
                        if (id >= g_arr_len || !g_arr[id].active) {
                            fail_and_exit("loadprog: inactive id");
                        }

                        //duplicate mem[B] into a fresh buffer
                        size_t n = g_arr[id].len;
                        uint32_t *dup = NULL;
                    
                        if (n > 0) {
                            dup = (uint32_t*)malloc(n * sizeof(uint32_t));
                            if (!dup) fail_and_exit("loadprog: OOM");
                            memcpy(dup, g_arr[id].data, n * sizeof(uint32_t));
                        }

                        // replace array 0's data
                        free(g_arr[0].data);
                        g_arr[0].data = dup;
                        g_arr[0].len = n;
                        g_arr[0].active = 1;
                    }
                    // jump: set pc = C (no increment)
                    pc = new_pc;
                    break;
                }

                default:
                    fail_and_exit("invalid opcode");
            }
        }
        if (trace) dump_reg_changes(before, regs);
    }
}