CC ?= cc
SRC = src/loader.c
BIN = loader

debug: CFLAGS = -std=c17 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wshadow
release: CFLAGS = -std=c17 -O3 -DNDEBUG -Wall -Wextra
perf: CFLAGS = -std=c17 -O3 -DNDEBUG -flto -Wall -Wextra

.PHONY: debug release perf clean
debug release perf:
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	rm -f $(BIN)

# ---------- Disassembler -------
disasm: src/disasm.c
	$(CC) -std=c17 -O0 -g -Wall -Wextra -Wshadow \
	-D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64 \
	-o disasm src/disasm.c