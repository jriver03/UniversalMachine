#ifndef _POSIX_CSOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

static void die(const char *msg) {
    fprintf(stderr, "asm: %s\n", msg);
    exit(1);
}

static FILE *xfopen(const char *path, const char *mode) {
    FILE *fp = fopen(path, mode);
    
    if (!fp) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        exit(1);
    }

    return fp;
}

int main(int argc, char **argv) {
    const char *in = NULL, *out = NULL;
    
    if (argc < 2) {
        fprintf(stderr, "usage: %s <input.uma> [-o output.um]\n", argv[0]);
        return 2;
    }

    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out =  argv[++i];
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return 2;
        }

        if (!out) { out = "a.um"; }

        FILE *fin = xfopen(in, "r");
        FILE *fout = xfopen(out, "wb");

        fclose(fin);
        fclose(fout);
        return 0;
    }
}