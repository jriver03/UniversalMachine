# Choose compiler
CC ?= clang

# Common warnings and standard
CFLAGS := -std=c17 -Wall -Wextra -Wshadow

# Debug vs Release toggle:
# make           -> debug build (sanitizers on)
# make RELEASE=1 -> release build (fast)
ifeq ($(RELEASE),1)
	CFLAGS += -O3 -DNDEBUG
else
	CFLAGS += -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
endif

# Your target program and its source(s)
BIN := loader
SRC := src/loader.c

.PHONY: all run clean

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Convenience: run the loader on a sample file
run: $(BIN)
	./$(BIN) programs/helloworld.um

clean:
	rm -f $(BIN)