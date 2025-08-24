#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

static void die(const char *msg) {
    fprintf(stderr, "disasm: %s\n", msg);
    exit(1);
}

/* assembe the 32 bit big endian word from 4 bytes (A is MSB)*/
static inline uint32_t be32_from(const unsigned char b[4]) {
    return ((uint32_t)b[0] << 24) |
           ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] <<  8) |
           ((uint32_t)b[3] <<  0);
}

static inline unsigned OPC(uint32_t w) { return w >> 28; } // 28..31
static inline unsigned ABC_A(uint32_t w) { return (w >> 6) & 7u; } // 6..8
static inline unsigned ABC_B(uint32_t w) { return (w >> 3) & 7u; } // 3..5
static inline unsigned ABC_C(uint32_t w) { return (w >> 0) & 7u; } // 0..2
static inline unsigned LI_A(uint32_t w) { return (w >> 25) & 7u; } // 25..27
static inline unsigned LI_VAL(uint32_t w) { return w & 0x1FFFFFFu; } //0..24

/* read all big endian words from a .um file. returns malloc buffer. */
static uint32_t *read_um(const char *path, size_t *out_nwords) {
    FILE *fp = fopen(path, "rb");
    
    if (!fp) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        return NULL;
    }

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

static void print_insn(uint32_t w, size_t pc) {
    unsigned op = OPC(w);

    printf(";; [pc=%zu word=0x%08x]\n", pc, w);

    if (op == 13u) {
        unsigned A = LI_A(w);
        uint32_t imm = LI_VAL(w);
        printf("loadimm %u %u\n", A, imm);
        return;
    }

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

        default:
            printf(";; UNKNOWN op=%u (raw=0x%08x)\n", op, w);
    }
}

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