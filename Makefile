# ---- config ----
CC ?= cc
PROG = loader
DISASM = disasm
ASM = asm

WARN = -Wall -Wextra -Wshadow

DBGFLAGS = -std=c17 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
RELFLAGS = -std=c17 -O3 -DNDEBUG
PERFFLAG = -flto

CFLAGS_COMMON = $(WARN)
LDFLAGS_COMMON = 

SRC_DIR = src
BUILD = BUILD
SRCS = $(SRC_DIR)/loader.c
OBJS = $(BUILD)/loader.o
DEPS = $(OBJS:.o=.d)

DISASM_SRCS = $(SRC_DIR)/disasm.c
DISASM_OBJS = $(BUILD)/disasm.o
DISASM_DEPS = $(DISASM_OBJS:.o=.d)

ASM_SRCS = $(SRC_DIR)/asm.c
ASM_OBJS = $(BUILD)/asm.o
ASM_DEPS = $(ASM_OBJS:.o=.d)

#default
.PHONY: all
all: debug

# ---- build modes ----
.PHONY: debug release perf
debug: $(BUILD)/$(PROG)

release: $(BUILD)/$(PROG)-release
perf: $(BUILD)/$(PROG)-perf

# ---- link steps ----
$(BUILD)/$(PROG): $(OBJS) | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(DBGFLAGS) $(LDFLAGS_COMMON) -o $@ $^

$(BUILD)/$(PROG)-release: $(OBJS:.o=-rel.o) | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(RELFLAGS) $(LDFLAGS_COMMON) -o $@ $^

$(BUILD)/$(PROG)-perf: $(OBJS:.o=-perf.o) | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(RELFLAGS) $(LDFLAGS_COMMON) $(PERFFLAG) -o $@ $^

# Disassembler & assembler (debug-flavored by default)
.PHONY: disasm asm
disasm: $(BUILD)/$(DISASM)
asm: $(BUILD)/$(ASM)

$(BUILD)/$(DISASM): $(DISASM_OBJS) | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(DBGFLAGS) $(LDFLAGS_COMMON) -o $@ $^

$(BUILD)/$(ASM): $(ASM_OBJS) | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(DBGFLAGS) $(LDFLAGS_COMMON) -o $@ $^

# ---- compile rules ----
$(BUILD):
	mkdir -p $(BUILD)

# Debug objects
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(DBGFLAGS) -MMD -MP -c $< -o $@

# Release/perf variants reuse same source
$(BUILD)/%-rel.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(RELFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%-perf.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(RELFLAGS) $(PERFFLAG) -MMD -MP -c $< -o $@

# ---- tests ----
.PHONY: test
test: debug
	@tests/run.sh

# ---- clean ----
.PHONY: clean
clean:
	rm -rf $(BUILD)

# ---- deps ----
-include $(DEPS) $(DISASM_DEPS) $(ASM_DEPS)