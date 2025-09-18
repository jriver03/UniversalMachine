"""
assembler.py

Julian Fong CS554

assembles assembly code into .um files 

"""

import sys
import struct

"""
opcodes for every
instruction 
"""

opcodes = {
    "cmov": 0,
    "aidx": 1,
    "aupd": 2,
    "add": 3,
    "mul": 4,
    "div": 5,
    "nand": 6,
    "halt": 7,
    "alloc": 8,
    "dealloc": 9,
    "out": 10,
    "in": 11,
    "loadprog": 12,
    "loadimm": 13,
}


"""
This maps how some instructions 
use differenent register combinations
"""
formats = {
    "cmov":    "abc",
    "aidx":    "abc",
    "aupd":    "abc",
    "add":     "abc",
    "mul":     "abc",
    "div":     "abc",
    "nand":    "abc",
    "halt":    "",
    "alloc":   "bc",   # r[b]=id, r[c]=size
    "dealloc": "c",    # r[c]=id
    "out":     "c",
    "in":      "c",
    "loadprog":"bc",   # r[b]=arrayid, r[c]=offset
    "loadimm": "ai",   # r[a]=imm
}

def encode_r(op, a, b, c):
    """ Creates the instruction in binary format and puts 
    registers into correct postions 
    """
    return (op << 28) | (a << 6) | (b << 3) | c

def encode_loadimm(a, imm):
    """ exception case for the special instruction 
    loadimm """
    return (13 << 28) | (a << 25) | (imm & 0x1FFFFFF)

def assemble_instr(instruc, args, labels):
    if instruc == "loadimm":
        a = args[0]
        imm = args[1]
        return encode_loadimm(a, imm)
    else:
        op = opcodes[instruc]
        fmt = formats[instruc]
        a = b = c = 0
        # Assign registers based on format string
        # 'a', 'b', 'c' correspond to registers a,b,c
        # args are in order of operands
        # fill missing with 0
        while len(args) < len(fmt):
            args.append(0)
        for i, ch in enumerate(fmt):
            if ch == 'a':
                a = args[i]
            elif ch == 'b':
                b = args[i]
            elif ch == 'c':
                c = args[i]
        return encode_r(op, a, b, c)

def assemble(lines):
    labels = {}
    pc = 0
    # Pass 1: collect labels and their corresponding line number 
    for line in lines:
        line = line.strip().split(";;")[0]
        if not line: continue
        tokens = line.split()
        if tokens[0] == "label":
            labels[tokens[1]] = pc
        else:
            pc += 1

    # Pass 2: encode instructions 
    program = []
    pc = 0
    for line in lines:
        line = line.strip().split(";;")[0]
        if not line: continue
        tokens = line.split()
        if tokens[0] == "label":
            continue
        instr = tokens[0]
        args = []
        for t in tokens[1:]:
            if t.startswith("@"):
                args.append(labels[t])
            else:
                args.append(int(t))
        word = assemble_instr(instr, args, labels)
        program.append(word)
        pc += 1
    return program

if __name__ == "__main__":
    lines = open(sys.argv[1]).read().splitlines()
    program = assemble(lines)
    with open(sys.argv[2], "wb") as f:
        for word in program:
            f.write(struct.pack(">I", word))  # big-endian