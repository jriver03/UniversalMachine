// UM Disassembler (Warmup 2)
// ------------------------------------------------------------
// Single-file disassembler for the "Universal Machine" ISA
// as described in machine-specification.pdf.
//
// Input : a .um binary (big-endian 32-bit words)
// Output: a readable assembly listing to stdout (one insn per line)
//         with a small comment header line before each instruction:
//
//     ;; [pc=<index> word=0xXXXXXXXX]
//     <mnemonic> <operands...>
//
// Fielding recap (matches the emulator/assembler):
//   - op = bits 28..31
//   - ABC layout: A=6..8, B=3..5, C=0..2
//   - loadimm (op=13): A=25..27, imm=0..24
//
// Notes:
//   - We keep the textual mnemonics identical to our assembler.
//   - Non-ABC ops print only the operands that are actually used,
//     so e.g. `out C`, `in C`, `halt`, `alloc B C`, `dealloc C`.
//   - Unknown opcodes are printed as a comment with the raw word.
//
// CLI:
//   usage: disasm <program.um>
//
// Error handling: fail fast with a short diagnostic.
// ------------------------------------------------------------
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // expose POSIX getline/fseeko/ftello

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif // 64 bit off_t for large files

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

/*--------------------------- tiny fail helper ----------------------------*/
static void die(const char *msg) {
    fprintf(stderr, "disasm: %s\n", msg);
    exit(1);
}

/*------------------------ word/bitfield utilities ------------------------*/

/* assembe the 32 bit big endian word from 4 bytes (A is MSB)*/
static inline uint32_t be32_from(const unsigned char b[4]) {
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) |
           ((uint32_t)b[3] <<  0);
}

/* field extractors */
static inline unsigned OPC(uint32_t w) { return w >> 28; } // 28..31
static inline unsigned ABC_A(uint32_t w) { return (w >> 6) & 7u; } // 6..8
static inline unsigned ABC_B(uint32_t w) { return (w >> 3) & 7u; } // 3..5
static inline unsigned ABC_C(uint32_t w) { return (w >> 0) & 7u; } // 0..2
static inline unsigned LI_A(uint32_t w) { return (w >> 25) & 7u; } // 25..27
static inline unsigned LI_VAL(uint32_t w) { return w & 0x1FFFFFFu; } //0..24

/*-------------------------- .um file ingestion ---------------------------*/
/* Read all big-endian words from a .um file into a malloc'd buffer.
   On success:
     - returns pointer to words
     - *out_nwords set to number of 32-bit words
   On error: prints a message and returns NULL. */
static uint32_t *read_um(const char *path, size_t *out_nwords) {
    FILE *fp = fopen(path, "rb");
    
    if (!fp) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }

// find file size (seek to end then ftello)
    if (fseeko(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        die("fseeko failed");
    }

    off_t size = ftello(fp);

    if (size < 0) {
        fclose(fp);
        die("ftello failed");
    }

    if (fseeko(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        die("rewind failed");
    }

    // .um must be nonempty and multiple of 4 bytes
    if (size == 0 || (size & 3) != 0) {
        fclose(fp);
        die(".um size invalid");
    }

    size_t n =  (size_t)(size / 4);

    uint32_t *words = (uint32_t*)malloc(n * sizeof(uint32_t));

    if (!words) {
        fclose(fp);
        die("out of memory");
    }

    // read 4 bytes -> big-endian assembe -> store
    unsigned char buf[4];
    for (size_t i = 0; i < n; ++i) {
        if (fread(buf, 1, 4, fp) != 4) {
            free(words);
            fclose(fp);
            die("short read");
        }
        words[i] = be32_from(buf);
    }

    fclose(fp);
    *out_nwords = n;
    return words;
}


/*--------------------------- pretty-print one ----------------------------*/
/* Decode one 32-bit word and print an assembly line (plus a header). */
static void print_insn(uint32_t w, size_t pc) {
    unsigned op = OPC(w);

    // small header comment with PC + raw word
    printf(";; [pc=%zu word=0x%08x]\n", pc, w);

    // special layout: loadimm (op = 13)
    if (op == 13u) {
        unsigned A = LI_A(w);
        uint32_t imm = LI_VAL(w);
        printf("loadimm %u %u\n", A, imm);
        return;
    }

    // standard ABC layout for all other ops
    unsigned A = ABC_A(w), B = ABC_B(w), C = ABC_C(w);

    switch (op) {
        case 0: {
            printf("cmov %u %u %u\n", A, B, C);
            break;
        }
        case 1: {
            printf("aidx %u %u %u\n", A, B, C);
            break;
        }
        case 2: {
            printf("aupd %u %u %u\n", A, B, C);
            break;
        }
        case 3: {
            printf("add %u %u %u\n", A, B, C);
            break;
        }
        case 4: {
            printf("mul %u %u %u\n", A, B, C);
            break;
        }
        case 5: {
            printf("div %u %u %u\n", A, B, C);
            break;
        }
        case 6: {
            printf("nand %u %u %u\n", A, B, C);
            break;
        }
        case 7: {
            printf("halt\n");
            break;
        }
        case 8: {
            printf("alloc %u %u\n", B, C);
            break;
        }
        case 9: {
            printf("dealloc %u\n", C);
            break;
        }
        case 10: {
            printf("out %u\n", C);
            break;
        }
        case 11: {
            printf("in %u\n", C);
            break;
        }
        case 12: {
            printf("loadprog %u %u\n", B, C);
            break;
        }

        // keep going, dump unknowns but don't crash
        default:
            printf(";; UNKNOWN op=%u (raw=0x%08x)\n", op, w);
    }
}

/*---------------------------------- main ---------------------------------*/
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <program.um>\n", argv[0]);
        return 2;
    }

    size_t n = 0;
    uint32_t *w = read_um(argv[1], &n);

    if (!w) {
        return 1;
    }

    for (size_t pc = 0; pc < n; ++pc) {
        print_insn(w[pc], pc);
    }

    free(w);
    return 0;
}