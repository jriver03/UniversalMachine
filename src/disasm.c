#define _POSIX_C_SOURCE 2000809L
#define _FILE_OFFSET_BITS 64
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

static void die(const char *msg) {
    fprintf(stderr, "disasm: %s\n", msg);
}

