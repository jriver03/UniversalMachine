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
        uint32_t r0;
        uint32_t r1;
        uint32_t r2;
        uint32_t r3;
        uint32_t r4;
        uint32_t r5;
        uint32_t r6;
        uint32_t r7;
        uint32_t pc;
        uint32_t len; /* Length of program in words */
    } r;
    uint32_t registers[10];
} REGISTERS;

/* Array of allocated arrays */
typedef struct Array {
    uint32_t* alloced_array;
    unsigned long array_len;
} Array;


/* Helps for reuse of array allocation ids */
typedef struct Stack {
    unsigned *buf;
    size_t top;   // number of elements in stack
    size_t cap;   // allocated capacity
} Stack;



