// UM Assembler (Warmup 2)
// ------------------------------------------------------------
// Single-file, two-pass assembler for the "Universal Machine" ISA
// as described in machine-specification.pdf.
//
// Pass 1: scan lines, record labels ("label @name") with the PC of
//         the *next* instruction.
// Pass 2: re-scan, parse mnemonics + operands, encode 32-bit words,
//         and write them in big-endian order to the output .um file.
//
// Supported mnemonics:
//   - ABC form: cmov aidx aupd add mul div nand
//   - specials: halt, alloc, dealloc, out, in, loadprog, loadimm
//
// Syntax notes:
//   - Registers: r0..r7 or 0..7
//   - Immediates for `loadimm`: decimal/hex/char literal or @label
//       Examples: 123, 0x7B, 'A', '\n', '\x41', @loop
//   - Labels:     `label @name`  (records current PC)
//   - Comments:   everything after ";;" on a line is ignored
//
// CLI:
//   usage: asm <input.uma> [-o output.um]
//   If -o is omitted, defaults to "a.um".
//
// Output format:
//   - Each instruction encoded as a single 32-bit word.
//   - Words are written big-endian (MSB first), as required by .um.
//
// Error handling: fails fast with line/column context when possible.
// ------------------------------------------------------------

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // expose POSIX getline, fseeko, etc.

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif // 64 bit off_t for large files

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

#if defined(__GNUC__)
# define NORETURN __attribute__((noreturn))
#else
# define NORETURN
#endif

/*--------------------------- tiny fail helpers ---------------------------*/
static void die(const char *msg) NORETURN;
static void die(const char *msg) {
    fprintf(stderr, "asm: %s\n", msg);
    exit(1);
}

/* formatted failure with file:line prefix */

static void failf(const char *file, int line, const char *fmt, ...) NORETURN;
static void failf(const char *file, int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "asm:%s:%d: ", file ? file : "<stdin>", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

/*------------------------ basic lexical helpers -------------------------*/

/* allow letters, digits, and a few punctuation chars in label names */
static int is_labelch(int c) {
    unsigned char uc = (unsigned char)c;
    return isalnum(uc) || uc=='_' || uc==':' || uc=='.' || uc=='-';
}

/* fopen with error message */
static FILE *xfopen(const char *path, const char *mode) {
    FILE *fp = fopen(path, mode);
    
    if (!fp) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        exit(1);
    }

    return fp;
}

/* remove trailing ";; comment" from a line */
static void strip_comment(char *s) {
    char *p = strstr(s, ";;");
    if (p) { *p = '\0'; }
}

/* trim right: newline, CR, or whitespace */
static void rstrip(char *s) {
    size_t n = strlen(s);

    while (n && (s[n-1]=='\n' || s[n-1]=='\r' || isspace((unsigned char)s[n-1]))) {
        s[--n] = '\0';
    }
}

/* trim left whitespace; returns first non-space */
static char *lstrip(char *s) {
    while (*s && isspace((unsigned char)*s)) { ++s; }
    return s;
}

/* true if line is empty after stripping */
static int is_blank(const char *s) { return *s == '\0'; }

/* parse "label @name" at start of line; write name into out_name */
static int parse_label(const char *line, char *out_name, size_t cap) {
    const char *p = line;
    const char *kw = "label";

    // match keyword "label"
    while (*kw && *p && *p == *kw) { 
        ++p;
        ++kw;
    }

    if (*kw != '\0') return 0;

    // require whitespace then '@'
    if (!isspace((unsigned char)*p)) return 0;
    while (isspace((unsigned char)*p)) ++p;

    if (*p != '@') return 0;

    ++p;

    // copy label characters
    size_t k = 0;
    while (*p && is_labelch(*p)) {
        if (k + 1 < cap) { out_name[k++] = *p; }
        ++p;
    }

    out_name[k] = '\0';
    return k > 0;
}

/*----------------------------- label table ------------------------------*/

typedef struct { 
    char *name; // strdup'd
    uint32_t pc; // instruction index (0-based)
} Label;

static Label *labels = NULL;
static size_t nlabels = 0, caplabels = 0;

/* append a label->pc mapping */
static void labels_add(const char *name, uint32_t pc) {
    if (nlabels == caplabels) {
        size_t nc = caplabels ? caplabels*2 : 16;
        Label *nl = (Label*)realloc(labels, nc * sizeof(Label));

        if (!nl) { die("oom labels"); }
        
        labels = nl;
        caplabels = nc;
    }

    labels[nlabels].name = strdup(name);
    
    if (!labels[nlabels].name) { die("oom strdup"); }
    
    labels[nlabels].pc = pc;
    nlabels++;
}

/* lookup; returns 1 if found and stores pc */
static int labels_find(const char *name, uint32_t *out_pc) {
    for (size_t i = 0; i < nlabels; ++i) {
        if (strcmp(labels[i].name, name) == 0) {
            *out_pc = labels[i].pc;
            return 1;
        }
    }
    return 0;
}

/* free entire label table */
static void labels_free(void) {
    for (size_t i = 0; i < nlabels; ++i) {
        free(labels[i].name);
    }
    free(labels);
        labels = NULL;
        nlabels = caplabels = 0;
}

/*------------------------- output word emission -------------------------*/

/* write a 32-bit word in big-endian byte order */
static void emit_be32(FILE *f, uint32_t w) {
    unsigned char b[4];
    
    b[0] = (unsigned char)(w >> 24);
    b[1] = (unsigned char)(w >> 16);
    b[2] = (unsigned char)(w >>  8);
    b[3] = (unsigned char)(w >>  0);

    if (fwrite(b, 1, 4, f) != 4) {
        die("write failed");
    }
}

/*---------------------------- token helpers -----------------------------*/

/* returns next comma/space-separated token, NULL if none.
   Mutates the input string: inserts '\0' after the token. */
static char* next_token(char *s, char **end) {
    while (*s && (isspace((unsigned char)*s) || *s == ',')) ++s;

    if (*s == '\0') { 
        *end = s;
        return NULL;
    }

    char *p = s;

    while (*p && !isspace((unsigned char)*p) && *p != ',') ++p;
    if (*p) { 
        *p = '\0';
        ++p;
    }

    *end = p;
    return s;
}

/* parse register token: r0..r7 or 0..7 */
static int parse_reg(const char *t, unsigned *out) {
    if (t[0] == 'r' || t[0] == 'R') ++t;
    char *e = NULL;
    long v = strtol(t, &e, 10);
    if (e == t || *e != '\0' || v < 0 || v > 7) { return 0; }

    *out = (unsigned)v;
    return 1;
}

/* parse immediate:
   - @label   -> resolve to label PC
   - 'c' or escaped char: '\n','\t','\r','\0','\\','\'','\xNN'
   - decimal or hex numeric literal (0x...)
*/
static int parse_imm(const char *t, uint32_t *out) {
    if (t[0] == '@') {
        uint32_t pc;
        if (!labels_find(t + 1, &pc)) return 0;
        *out = pc;
        return 1;
    }

    // character literal
    if (t[0] == '\'') {
        const char *p = t + 1;
        unsigned v = 0;

        if (*p == '\\') {
            ++p;
            if (*p == 'n') {
                v = '\n';
                ++p;
            } else if (*p == 't') { 
                v = '\t';
                ++p;
            } else if (*p == 'r') {
                v = '\r';
                ++p;
            } else if (*p == '0') {
                v = '\0';
                ++p;
            } else if (*p == '\\') {
                v = '\\';
                ++p;
            } else if (*p == '\'') {
                v = '\'';
                ++p;
            } else if (*p == 'x') {
                ++p;
                char *end = NULL;
                unsigned long xv = strtoul(p, &end, 16);

                if (end == p) return 0;

                v = (unsigned)xv;
                p = end;
            } else return 0;
        } else {
        v = (unsigned char)*p++;
        }
        if (*p != '\'') return 0;
        *out = v;
        return 1;
    }

    // numeric
    char *e = NULL;
    unsigned long v = strtoul(t, &e, 0);

    if (e == t || *e != '\0' || v > 0xFFFFFFFFu) return 0;
    *out = (uint32_t)v;
    return 1;
}

/*---------------------------------- main ---------------------------------*/
int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL;
    
    if (argc < 2) {
        fprintf(stderr, "usage: %s <input.uma> [-o output.um]\n", argv[0]);
        return 2;
    }

    in = argv[1];

    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out =  argv[++i];
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    if (!out) { out = "a.um"; }

    FILE *fin = xfopen(in, "r");
    FILE *fout = xfopen(out, "wb");

    /*------------------------------- Pass 1 -------------------------------*/
    // Scan file, collect labels with the PC (instruction count).

    uint32_t pc = 0; // increments per instruction
    char *line = NULL; // getline buffer
    size_t cap = 0;
    ssize_t got;

    while ((got = getline(&line, &cap, fin)) != -1) {
        strip_comment(line);
        rstrip(line);

        char *s = lstrip(line);

        if (is_blank(s)) continue;

        char name[128];

        if (parse_label(s, name, sizeof name)) {
            labels_add(name, pc); // label points to next instruction index
            continue; // labels don't consume PC
        }

        pc++; // count an instruction
    }

    free(line);

    // rewind for pass 2
    if (fseeko(fin, 0, SEEK_SET) != 0) {
        die("rewind failed");
    }

    /*------------------------------- Pass 2 -------------------------------*/
    // Re-scan, encode each instruction, and write big-endian words.
    char *line2 = NULL;
    size_t cap2 = 0;
    ssize_t got2;

    int lineno = 0;

    while ((got2 = getline(&line2, &cap2, fin)) != -1) {
        ++lineno;
        strip_comment(line2);
        rstrip(line2);

        char *s = lstrip(line2);

        if (is_blank(s)) continue;

        // skip label lines
        char name[128];
        if (parse_label(s, name, sizeof name)) continue;
        
        // mnemonic + operands
        char *rest = NULL;
        char *mn = next_token(s, &rest);

        if (!mn) { failf(in, lineno, "missing mnemonic"); }

        uint32_t word = 0;
        
        /* --- loadimm A IMM (special fielding: op=13, A in 25..27, imm in 0..24 --- */
        if (strcmp(mn, "loadimm") == 0) {
            unsigned A = 0;
            uint32_t imm = 0;

            char *tA = next_token(rest, &rest);
            char *tI = tA ? next_token(rest, &rest) : NULL;

            if (!tA || !tI || !parse_reg(tA, &A) || !parse_imm(tI, &imm)) {
                failf(in, lineno, "loadimm syntax: loadimm A IMM");
            }

            if (imm > 0x1FFFFFFu) { 
                failf(in, lineno, "loadimm immediate too large (needs 25 bits)"); 
            }

            word = (13u<<28) | ((A & 7u) << 25) | (imm & 0x1FFFFFFu);
        /* --- ABC form: cmov aidx aupd add mul div nand --- */
        } else if (strcmp(mn, "cmov") == 0 || strcmp(mn, "aidx") == 0 ||
                   strcmp(mn, "aupd") == 0 || strcmp(mn, "add") == 0 ||
                   strcmp(mn, "mul") == 0 || strcmp(mn, "div") == 0 ||
                   strcmp(mn, "nand") == 0) {
                    
                    unsigned A = 0, B = 0, C = 0;
                    char *tA = next_token(rest, &rest);
                    char *tB = tA ? next_token(rest, &rest) : NULL;
                    char *tC = tB ? next_token(rest, &rest) : NULL;

                    if (!tA || !tB || !tC || !parse_reg(tA, &A) || !parse_reg(tB, &B) || !parse_reg(tC, &C)) {
                        failf(in, lineno, "ABC syntax: op A B C (regs 0..7)");
                    }

                    unsigned op;

                    if (strcmp(mn, "cmov")==0) op=0;
                    else if (strcmp(mn, "aidx")==0) op=1;
                    else if (strcmp(mn, "aupd")==0) op=2;
                    else if (strcmp(mn, "add")==0) op=3;
                    else if (strcmp(mn, "mul")==0) op=4;
                    else if (strcmp(mn, "div")==0) op=5;
                    else op=6;

                    word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        /* --- halt (op=7, ABC fields unused/zero) --- */
        } else if (strcmp(mn, "halt")==0) {
            unsigned op = 7;
            unsigned A=0, B=0, C=0;

            word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        /* --- alloc B C (op=8, A unused/zero) --- */
        } else if (strcmp(mn, "alloc")==0) {
            unsigned B = 0, C = 0;

            char *tB = next_token(rest, &rest);
            char *tC = tB ? next_token(rest, &rest) : NULL;

            if (!tB || !tC || !parse_reg(tB, &B) || !parse_reg(tC, &C)) { 
                failf(in, lineno, "alloc syntax: alloc B C");
            }

            unsigned op=8, A=0;

            word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        /* --- dealloc C (op=9, A/B unused/zero) --- */
        } else if (strcmp(mn, "dealloc")==0) {
            unsigned C = 0;
            char *tC = next_token(rest, &rest);

            if (!tC || !parse_reg(tC, &C)) { 
                failf(in, lineno, "dealloc syntax: dealloc C");
            }

            unsigned op=9, A=0, B=0;

            word = (op<<28) | ((A&7u)<<6) |((B&7u)<<3) | (C&7u);
        /* --- out C (op=10) --- */
        } else if (strcmp(mn, "out")==0) {
            unsigned C = 0;
            char *tC = next_token(rest, &rest);

            if (!tC || !parse_reg(tC, &C)) { 
                failf(in, lineno, "out syntax: out C"); 
            }

            unsigned op=10, A=0, B=0;

            word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        /* --- in C (op=11) --- */
        } else if (strcmp(mn, "in")==0) {
            unsigned C = 0;
            char *tC = next_token(rest, &rest);

            if (!tC || !parse_reg(tC, &C)) { 
                failf(in, lineno, "in syntax: in C"); 
            }

            unsigned op=11, A=0, B=0;
            word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        /* --- loadprog B C (op=12, A unused/zero) --- */
        } else if (strcmp(mn, "loadprog")==0) {
            unsigned B=0, C=0;
            char *tB = next_token(rest, &rest);
            char *tC = tB ? next_token(rest, &rest) : NULL;

            if (!tB || !tC || !parse_reg(tB, &B) || !parse_reg(tC,&C)) { 
                failf(in, lineno, "loadprog syntax: loadprog B C"); 
            }
            unsigned op=12, A=0;

            word = (op<<28) | ((A&7u)<<6) | ((B&7u)<<3) | (C&7u);
        } else {
            failf(in, lineno, "unknown mnemonic '%s'", mn);
        }
        // emit the encoded word
        emit_be32(fout, word);
    }

    free(line2);
    labels_free();

    fclose(fin);
    fclose(fout);
    return 0;
}