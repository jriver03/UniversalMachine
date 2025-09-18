/*********************************/
/* machine.c                     */
/* CS554 Julian Fong             */
/* Simple machine emulator       */
/*********************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "machine.h"

// #define DEBUG 0 supplied from makefile
#define DBG(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__);

REGISTERS reg;
FILE* program_fp;
/* Will be used to identify newly allocated arrays */
/* Increment everytime you allocate a new array, use it as the ID */
unsigned int array_ids = 1;
Stack* ids_stack;

/* alloced array table */
static Array* array_table = NULL;
/* table capacity */
static size_t array_capacity = 0;

/* Reallocates alloced array array if we need more space */
static void ensure_capacity(unsigned int id)
{
    if (id >= array_capacity)
    {
        size_t new_capacity = array_capacity ? array_capacity : 16;
        while (new_capacity <= id)
            new_capacity *= 2;
        Array* new_table = (Array*) realloc(array_table, new_capacity * sizeof(Array));
        #if DEBUG
        if (!new_table)
        {
            fprintf(stderr, "Failed to realloc array_table\n");
            exit(EXIT_FAILURE);
        }
        #endif
        array_table = new_table;
        array_capacity = new_capacity;
    }
}

/* Push an id onto the free-id stack.
   Grows capacity automatically */
static inline void push_id(Stack* s, unsigned int id)
{
    if (s->top == s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 32;
        unsigned int *p = (unsigned int*)realloc(s->buf, nc * sizeof *p);
        #if DEBUG
        if (!p) { printf("Stack failed\n"); exit(1); }
        #endif
        s->buf = p;
        s->cap = nc;
    }
    s->buf[s->top++] = id;
}

/* Pop the most recent freed id; returns 0 if stack is empty. */
static inline unsigned int pop_id(Stack* s)
{
    return (s->top ? s->buf[--s->top] : 0U);
}

/* Free internal storage for the stack. */
static inline void free_stack(Stack* s)
{
    free(s->buf);
    s->buf = NULL;
    s->top = s->cap = 0;
}

/* Allocate a new array given an identifier */
/* size is given in words (32 bits) */
void malloc_array(unsigned int id, unsigned long size)
{
    ensure_capacity(id);
    array_table[id].alloced_array = (unsigned int*) calloc(size, sizeof(unsigned int));
    array_table[id].array_len = size;
}

/* Returns non zero on failure */
int dealloc_array(unsigned int id) {
    #if DEBUG
    if (id == 0) return 1;
    if (id >= array_capacity) return 1;
    #endif
    Array curr = array_table[id];
    #if DEBUG
    if (!curr.alloced_array) return 1; // not found
    array_table[id].alloced_array = NULL;
    #endif
    free(curr.alloced_array);
    return 0;
}

/* load_program: read program_words of 32-bit words and byte-swap each of them.
   Returns number of 32-bit words loaded. */
unsigned long load_program(unsigned long program_words)
{
    unsigned int *dst = array_table[0].alloced_array;
    unsigned int x;

    size_t n = fread(dst, sizeof(unsigned int), program_words, program_fp);
    if (n == 0 && ferror(program_fp)) 
    {
        printf("fread error\n");
        return 0;
    }

    for (size_t i = 0; i < n; ++i) 
    {
        x = dst[i];
        dst[i] = ((x & 0x000000FFU) << 24) |
                 ((x & 0x0000FF00U) <<  8) |
                 ((x & 0x00FF0000U) >>  8) |
                 ((x & 0xFF000000U) >> 24);
    }

    return n;
}

/* Reload the zero array, length given in 32 bit words */
void reload_program(unsigned int* new_array, unsigned long length)
{
    unsigned long byte_size = length * sizeof(unsigned int);
    unsigned int* temp;

    /* Reallocate if necessary */
    if(array_table[0].array_len < length) 
    {
        free(array_table[0].alloced_array);
        array_table[0].alloced_array = (unsigned int*) malloc(byte_size);
    }

    /* Copy program into zero array */
    memcpy(array_table[0].alloced_array, new_array, byte_size);
    array_table[0].array_len = length;
}

/* clean_exit: frees all allocated arrays, closes program file, and disposes the free-id stack. */
void clean_exit(Stack* bottom)
{
    Stack* id_to_free;
    /* Free all arrays in array_table */
    if (array_table)
    {
        for (size_t i = 0; i < array_capacity; i++)
        {
            if (array_table[i].alloced_array)
            {
                free(array_table[i].alloced_array);
                array_table[i].alloced_array = NULL;
            }
        }
        free(array_table);
        array_table = NULL;
        array_capacity = 0;
    }
    fclose(program_fp);

    /* Release vector-backed stack storage and the stack object itself. */
    free_stack(bottom);
    free(bottom);
}

/* execute: main VM loop.
   This version uses a computed-goto jump table (direct threading) for faster dispatch.*/
int execute(unsigned int* program)
{
    int opcode;
    int a, b, c;  /* Register indexes */
    unsigned int* word_array;
    unsigned int program_instr;
    unsigned int input;
    unsigned int alloc_size;
    unsigned int* R = reg.registers; /* Local copy to avoid mem accesses */

    /* Jump table: index by opcode (0..13). Index 14 for invalid opcodes. */
    static void* const jump_table[] = 
    {
        &&op0, &&op1, &&op2, &&op3, &&op4, &&op5, &&op6,
        &&op7, &&op8, &&op9, &&op10, &&op11, &&op12, &&op13,
        &&op_invalid
    };

    /* Helpers: fetch current instruction, then dispatch. */
    #define FETCH()   do { program_instr = program[reg.r.pc]; opcode = (program_instr >> 28); } while (0)
    #define DISPATCH() do { goto *jump_table[ /*(opcode <= 13) ? opcode : 14*/ opcode];} while (0)
    #define NEXT()    do { reg.r.pc++; FETCH(); DISPATCH(); } while (0)

    FETCH();
    DISPATCH();

op0: /* Move */
{
    a = (program_instr >> 6) & REG_MASK;  /* 8:6 */
    b = (program_instr >> 3) & REG_MASK;  /* 5:3 */
    c = (program_instr >> 0) & REG_MASK;  /* 2:0 */
    #if DEBUG
        DBG("cmov %u %u %u\n", a, b, c);
        NEXT();
    #endif
    if (R[c] != 0) R[a] = R[b];
    NEXT();
}

op1: /* Array index */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("aidx %u %u %u\n", a, b, c);
        NEXT();
    #endif
    word_array = array_table[R[b]].alloced_array;
    #if ERR_MSG
    if (word_array == NULL)
    {
        printf("Tried to index array id:%u that is not allocated\n", R[b]);
        return -1;
    }
    if (R[c] >= array_table[R[b]].array_len)
    {
        printf("Index %u does not exist for array of length %lu\n", R[c], array_table[R[b]].array_len);
        return -1;
    }
    #endif
    R[a] = word_array[R[c]];
    NEXT();
}

op2: /* Array Update */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("aupd %u %u %u\n", a, b, c);
        NEXT();
    #endif
    word_array = array_table[R[a]].alloced_array;
    #if ERR_MSG
    if (word_array == NULL)
    {
        printf("Tried to index array id:%u that is not allocated\n", R[a]);
        return -1;
    }
    if (R[b] >= array_table[R[a]].array_len)
    {
        printf("On line %u: Index %u out of bounds for array length %lu\n",
               reg.r.pc+1, R[b], array_table[R[a]].array_len);
        printf("This is array id=%u\n", R[a]);
        return -1;
    }
    #endif
    word_array[R[b]] = R[c];
    NEXT();
}

op3: /* Addition */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("add %u %u %u\n", a, b, c);
        NEXT();
    #endif
    R[a] = R[b] + R[c];
    NEXT();
}

op4: /* Mult */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("mul %u %u %u\n", a, b, c);
        NEXT();
    #endif
    R[a] = R[b] * R[c];
    NEXT();
}

op5: /* Division */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("div %u %u %u\n", a, b, c);
        NEXT();
    #endif
    #if ERR_MSG
    if (R[c] == 0)
    {
        printf("Division by zero. program offset = %u\n", reg.r.pc);
        return -1;
    }
    #endif
    R[a] = R[b] / R[c];
    NEXT();
}

op6: /* NAND */
{
    a = (program_instr >> 6) & REG_MASK;
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("nand %u %u %u\n", a, b, c);
        NEXT();
    #endif
    R[a] = ~(R[b] & R[c]);
    NEXT();
}

op7: /* Halt */
{
    #if DEBUG
        DBG("halt\n");
    #endif
    #if ERR_MSG
        printf("exit code 0\n");
    #endif
    return 0;
}

op8: /* Alloc */
{
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("alloc %u %u\n", b, c);
        NEXT();
    #endif
    alloc_size = R[c]; /* Save the value if reg b == c*/
    if (ids_stack->top != 0)        /* If the stack isn't empty */
        R[b] = pop_id(ids_stack);   /* Reuse a freed ID */
    else
        R[b] = array_ids++;         /* Issue a new ID */

    malloc_array(R[b], alloc_size);
    NEXT();
}

op9: /* De-alloc */
{
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("dealloc %u\n", c);
        NEXT();
    #endif
    #if ERR_MSG
    if (dealloc_array(R[c]))
    {
        printf("Error, cannot deallocate for ID%u array\n", R[c]);
        return 1;
    }
    #else
    dealloc_array(R[c]);
    #endif
    push_id(ids_stack, R[c]);
    NEXT();
}

op10: /* Print */
{
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("out %u\n", c);
        NEXT();
    #endif
    #if ERR_MSG
    if (R[c] > 255)
    {
        printf("Attempted to print value of %u!\n", R[c]);
        return 1;
    }
    #endif
    putchar(R[c]);
    NEXT();
}

op11: /* INPUT */
{
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("in %u\n", c);
        NEXT();
    #endif
    R[c] = fgetc(stdin);
    NEXT();
}

op12: /* LOAD PROGRAM */
{
    b = (program_instr >> 3) & REG_MASK;
    c = (program_instr >> 0) & REG_MASK;
    #if DEBUG
        DBG("loadprog %u %u\n", b, c);
        NEXT();
    #endif
    word_array = array_table[R[b]].alloced_array;
    #if ERR_MSG
    if (word_array == NULL)
    {
        printf("Could not load from array %u\n", R[b]);
        return 1;
    }
    #endif
    if (R[b]) /* It wasn't a jump actually reload the program */
    {
        reload_program(word_array, array_table[R[b]].array_len);
        program = array_table[0].alloced_array;
        reg.r.len = array_table[R[b]].array_len;
    }
    reg.r.pc = R[c];
    FETCH();
    DISPATCH();
}

op13:
{
    a = (program_instr >> 25) & REG_MASK;
    input = program_instr & LOAD_MASK;
    #if DEBUG
        DBG("loadimm %u %u\n", a, input);
        NEXT();
    #endif
    R[a] = input;
    NEXT();
}

op_invalid:
{
    printf("%u: OPCODE=%u is invalid\n", reg.r.pc, opcode);
    reg.r.pc++;
    FETCH();
    DISPATCH();
}

    /* Unreachable */
    return 0;
}

int main(int argc, char** argv)
{
    int program_words;
    int exit_code = 0;

    if (argc < 2)
    {
        printf("No program file supplied\n");
        return 1;
    }

    if ((program_fp = fopen(argv[1], "rb")) == NULL)
    {
        printf("Could not find program.\n");
        return -1;
    }

    /* Get program size */
    #if 1
    fseek(program_fp, 0, SEEK_END);
    program_words = ftell(program_fp) / 4;
    rewind(program_fp);
    #else
    program_words = 14091; //Hard code the sandmark size
    #endif

    /* Load and initialize the program */
    malloc_array(0, program_words);
    /* Read program_words words and swap bytes then set actual length read */
    reg.r.len = load_program(program_words);

    /* Initialize free-id stack */
    ids_stack = (Stack*) malloc(sizeof(Stack));
    ids_stack->buf = NULL;
    ids_stack->top = 0;
    ids_stack->cap = 0;

    /* Begin execution loop */
    exit_code = execute(array_table[0].alloced_array);

    #if DEBUG
    clean_exit(ids_stack);
    if(exit_code) printf("Program fail. exit code %d\n", exit_code);
    #endif
    return exit_code;
}