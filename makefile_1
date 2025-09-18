# ---- config ----
CC ?= cc
PROG = loader
DISASM = disasm
ASM = asm

WARN = -Wall -Wextra -Wshadow

DBGFLAGS = -std=c17 -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DTRACE
RELFLAGS = -std=c17 -O3 -DNDEBUG
PERFFLAG = -flto

CFLAGS_COMMON = $(WARN) -Iinclude
CFLAGS_BASE = -std=c17 -Wall -Wextra
CFLAGS_DBG = $(CFLAGS_BASE) -O0 -g -DTRACE
CFLAGS_PERF = $(CFLAGS_BASE) -O3 -DNDEBUG -fomit-frame-pointer -march=native
LDFLAGS_COMMON = 
LDFLAGS_PERF = -flto

BUILD = BUILD

SRC_LOADER = src/loader.c
SRC_DIR = src
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

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Debug objects
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS_COMMON) $(DBGFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/loader: $(SRC_LOADER) | $(BUILD_DIR)
	$(CC) $(CFLAGS_DBG) $^ -o $@

$(BUILD_DIR)/loader-perf: $(SRC_LOADER) | $(BUILD_DIR)
	$(CC) $(CFLAGS_PERF) $^ -o $@ $(LDFLAGS_PERF)

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

PREFIX ?= /usr/local

.PHONY: help install uninstall
help:
	@echo "Targets:"
	@echo "  debug (default)  - Build with ASan/UBSan"
	@echo "  release          - Optimized build"
	@echo "  perf             - Optimized LTO build"
	@echo "  disasm asm       - Build utilities"
	@echo "  test             - Run tests (optional)"
	@echo "  clean            - Remove build artifacts"
	@echo "  install          - Install binaries to $(PREFIX)/bin"
	@echo "  uninstall        - Remove installed binaries"

install: all disasm asm
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 BUILD/loader  "$(DESTDIR)$(PREFIX)/bin/um"
	install -m 0755 BUILD/disasm  "$(DESTDIR)$(PREFIX)/bin/um-disasm"
	install -m 0755 BUILD/asm     "$(DESTDIR)$(PREFIX)/bin/um-asm"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/um" \
	      "$(DESTDIR)$(PREFIX)/bin/um-disasm" \
	      "$(DESTDIR)$(PREFIX)/bin/um-asm"