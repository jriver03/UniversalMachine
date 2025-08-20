#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

/* Consistent error exits */
static void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

/* Assemble a 32-bit word from 4 raw bytes, with A as the most-significant byte. */
static inline uint32_t be32_from(const unsigned char b[4]) {
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) |
           ((uint32_t)b[3] << 0);
}

static inline unsigned OPC(uint32_t w) { return w >> 28; } // bits 28..31
static inline unsigned ABC_A(uint32_t w) { return (w >> 6) & 7u; } // bits 6..8
static inline unsigned ABC_B(uint32_t w) {return (w >> 3) & 7u; } // bits 3..5
static inline unsigned ABC_C(uint32_t w) { return (w >> 0) & 7u; } // bits 0..2
static inline unsigned LI_A(uint32_t w) { return (w >> 25) & 7u; } // bits 25..27
static inline unsigned LI_VAL(uint32_t w) {return w & 0x1FFFFFFu; } // bits 0..24

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usuage: %s <program.um>\n", argv[0]);
        return 2; /* distingush CLI misues from I/O/runtime failures */
    }

    const char *path = argv[1];

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
    /** 
    size_t preview = nwords < 8 ? nwords : 8;
    for (size_t i = 0; i < preview; ++i) {
        printf("%02zu: 0x%08x\n", i, words[i]);
    }

    Decode trace
    printf("-- decode trace (first up to 20 words) --\n");
    size_t steps = nwords < 20 ? nwords : 20;
    for (size_t trace_pc = 0; trace_pc < steps; ++trace_pc) {
        uint32_t w = words[trace_pc];
        unsigned op = OPC(w);
        if (op == 13u) {
            printf("pc=%zu op=13 A=%u imm=%u (0x%08x)\n", trace_pc, LI_A(w), LI_VAL(w), LI_VAL(w));
        } else {
            printf("pc=%zu op=%u A=%u B=%u C=%u\n", trace_pc, op, ABC_A(w), ABC_B(w), ABC_C(w));
        }
    }
    */

    uint32_t regs[8] = {0}; // 8 general-purpose registers
    uint32_t pc = 0; // Program counter starts at 0
    uint32_t *mem0 = words; // Array 0 points to loaded program
    size_t mem0_len = nwords;

    for (;;) {
        if (pc >= mem0_len) {
            fprintf(stderr, "PC out of bounds\n");
            free(words);
            return 1;
        }

        uint32_t w = mem0[pc];
        unsigned op =  w >> 28; // bits 28..31

        unsigned A = 0, B = 0, C = 0;
        
        if (op == 13u) {
            unsigned a = (w >> 25) & 7u;
            uint32_t imm = w & 0x1FFFFFFu; // bits 0..24
            regs[a] = imm;
            pc++;
            continue;
        }

        A = (w >> 6) & 7u;
        B = (w >> 3) & 7u;
        C = w & 7u;

        switch (op) {

            case 7: { // Halt
                free(words);
                return 0; // Machine stops computation
            }

            case 10: { // Output
                uint32_t v = regs[C];

                if (v > 255u) { // Output must be 0..255
                    fprintf(stderr, "Output >255\n");
                    free(words);
                    return 1;
                }

                putchar((int)(v & 0xFF));
                fflush(stdout);
                pc++; // PC advances after execution (except op 12)
                break;
            }

            default:
                fprintf(stderr, "Unimplemented opcode %u at pc=%u\n", op, pc);
                free(words);
                return 1; // Valid instructions required
        }
        
    }

    free(words);
    return 0;
}