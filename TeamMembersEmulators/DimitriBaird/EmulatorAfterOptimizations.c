#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NUM_REGS 8
#define WORD uint32_t
#define BYTE uint8_t

WORD registers[NUM_REGS];
WORD **arrays;
WORD *arraySizes;
int capacity;
int *freeIDs;
int freeTop;
int nextID = 1;
WORD PC = 0;

void fail(const char *msg) {
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

//Ensure array table can hold arrayID
void ensure_capacity(int id) {
    if (id >= capacity) {
        int newCap = capacity * 2;
        arrays = realloc(arrays, newCap * sizeof(WORD *));
        arraySizes = realloc(arraySizes, newCap * sizeof(WORD));
        freeIDs = realloc(freeIDs, newCap * sizeof(int));
        for (int i = capacity; i < newCap; i++) {
            arrays[i] = NULL;
            arraySizes[i] = 0;
        }
        capacity = newCap;
    }
}


int main(int argc, char *argv[]) {
    int arrID;
    int index;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s program.um\n", argv[0]);
        return 1;
    }

    static void *jump_table[] = {
        &&op_condmove, &&op_array_index, &&op_array_update,
        &&op_add, &&op_mul, &&op_div, &&op_nand,
        &&op_halt, &&op_alloc, &&op_free,
        &&op_output, &&op_input, &&op_load_prog,
        &&op_load_immediate
    };

    memset(registers, 0, sizeof(registers));
    capacity = 1024; // initial array table size
    arrays = calloc(capacity, sizeof(WORD *));
    arraySizes = calloc(capacity, sizeof(WORD));
    freeIDs = malloc(capacity * sizeof(int));
    freeTop = 0;

    //load program file into array 0
    FILE *f = fopen(argv[1], "rb");
    if (!f) fail("Cannot open program file");

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    if (size % 4 != 0) fail("Program size not divisible by 4");
    long words = size / 4;

    WORD *program = malloc(words * sizeof(WORD));
    for (long i = 0; i < words; i++) {
        BYTE A = fgetc(f);
        BYTE B = fgetc(f);
        BYTE C = fgetc(f);
        BYTE D = fgetc(f);
        program[i] = ((WORD)A << 24) | ((WORD)B << 16) | ((WORD)C << 8) | (WORD)D;
    }
    fclose(f);

    ensure_capacity(0);
    arrays[0] = program;
    arraySizes[0] = words;


    //run emulator loop
    while (1) {
        if (PC >= arraySizes[0]) fail("PC out of range");
        WORD instr = arrays[0][PC];
        int A = (instr >> 6) & 0x7;
        int B = (instr >> 3) & 0x7;
        int C = instr & 0x7;
        int opcode = instr >> 28;
        goto *jump_table[opcode];

        op_condmove:
            if (registers[C] != 0) {
                registers[A] = registers[B];
            }
            PC++;
            continue;

        op_array_index:
            arrID = registers[B];
            index = registers[C];
            if (arrID >= capacity || arrays[arrID] == NULL || index >= arraySizes[arrID])
                fail("Invalid array index");
            registers[A] = arrays[arrID][index];
            PC++;
            continue;

        op_array_update:
            arrID = registers[A];
            index = registers[B];
            if (arrID >= capacity || arrays[arrID] == NULL || index >= arraySizes[arrID])
                fail("Invalid array update");
            arrays[arrID][index] = registers[C];
            PC++;
            continue;

        op_add:
            registers[A] = registers[B] + registers[C];
            PC++;
            continue;

        op_mul:
            registers[A] = registers[B] * registers[C];
            PC++;
            continue;

        op_div:
            if (registers[C] == 0) fail("Division by zero");
            registers[A] = registers[B] / registers[C];
            PC++;
            continue;

        op_nand:
            registers[A] = ~(registers[B] & registers[C]);
            PC++;
            continue;

        op_halt:
            exit(0);
        op_alloc:
            WORD size = registers[C];
            WORD *newArray = calloc(size, sizeof(WORD));
            int newID;
            if (freeTop > 0) {
                newID = freeIDs[--freeTop];
            } else {
                newID = nextID++;
            }
            ensure_capacity(newID);
            arrays[newID] = newArray;
            arraySizes[newID] = size;
            registers[B] = newID;
            PC++;
            continue;

        op_free:
            arrID = registers[C];
            if (arrID == 0 || arrID >= capacity || arrays[arrID] == NULL)
                fail("Invalid deallocation");
            free(arrays[arrID]);
            arrays[arrID] = NULL;
            arraySizes[arrID] = 0;
            freeIDs[freeTop++] = arrID;
            PC++;
            continue;

        op_output:
            WORD value = registers[C];
            if (value > 255) fail("Output out of range");
            putchar((char)value);
            fflush(stdout);
            PC++;
            continue;

        op_input:
            int ch = getchar();
            if (ch == EOF) {
                registers[C] = 0xFFFFFFFF;
            } else {
                registers[C] = (WORD)ch;
            }
            PC++;
            continue;

        op_load_prog:
            arrID = registers[B];
            WORD offset = registers[C];
            if (arrID >= capacity || arrays[arrID] == NULL)
                fail("Invalid load program");
            if (arrID != 0) {
                // deep copy
                WORD size = arraySizes[arrID];
                WORD *dup = malloc(size * sizeof(WORD));
                memcpy(dup, arrays[arrID], size * sizeof(WORD));
                free(arrays[0]);
                arrays[0] = dup;
                arraySizes[0] = size;
            }
            PC = offset;
            continue;

        op_load_immediate:
            A = (instr >> 25) & 0x7;
            value = instr & 0x1FFFFFF;
            registers[A] = value;
            PC++;
            continue;

    }

    return 0;
}
