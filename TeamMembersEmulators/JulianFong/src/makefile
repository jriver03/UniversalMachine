
SRC=machine.c
HEADER=machine.h
# Flags that compile for absolute speed 
FAST_CFLAGS = -O2 -flto -fomit-frame-pointer -funroll-loops -ftree-vectorize

.PHONY: clean all tar

all: machine dasm

machine: $(SRC) $(HEADER)
	gcc -g $(SRC) -o machine -DDEBUG=0 -DERR_MSG=1

dasm: $(SRC) $(HEADER)
	gcc -g $(SRC) -o dasm -DDEBUG=1 -DERR_MSG=0

fast: $(SRC) $(HEADER)
	gcc $(FAST_CFLAGS) $(SRC) -o machine -DDEBUG=0 -DERR_MSG=0

tar:
	tar -cvf Julian_Fong_warmup_3.tar assembler.py disassembled/ machine.h machine.c makefile README.txt tests/

#
# test
#
# tests the assembler and disassembler
# will diff the assembled product with the .um file given by the professor
# will diff the dissassembled and assembled product with the same .um file.
test: machine dasm 
	python assembler.py disassembled/square.uma square_test.um
	diff square_test.um tests/square.um
	./dasm square_test.um > disassembled/square_test.uma
	python assembler.py disassembled/square_test.uma square_dis_test.um
	diff square_dis_test.um tests/square.um

clean:
	rm machine
	rm dasm
