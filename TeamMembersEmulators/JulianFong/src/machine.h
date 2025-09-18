/*********************************/
/* machine.h                     */
/* CS554 Julian Fong             */
/* Data types for machine        */
/*********************************/

/**************/
/* Constants  */
/**************/

#define REG_MASK        0x0007 /* This will get 3 bits for the register id */
#define LOAD_MASK       0x01FFFFFF /* 0:24, 25 bit number */
#define PC_IDX          9 /* The index of the PC register */

/**************/
/* Data Types */
/**************/

typedef union {
    struct {
        unsigned int r0;
        unsigned int r1;
        unsigned int r2;
        unsigned int r3;
        unsigned int r4;
        unsigned int r5;
        unsigned int r6;
        unsigned int r7;
        unsigned int pc;
        unsigned int len; /* Length of program in words */
    } r;
    unsigned int registers[10];
} REGISTERS;

/* Array of allocated arrays */
typedef struct Array {
    unsigned int* alloced_array;
    unsigned long array_len;
} Array;


/* Helps for reuse of array allocation ids */
typedef struct Stack {
    unsigned *buf;
    size_t top;   // number of elements in stack
    size_t cap;   // allocated capacity
} Stack;



