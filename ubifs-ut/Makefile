# SPDX-License-Identifier: GPL-2.0-only
#
# User-space unit-test framework for fs/ubifs.
#
# The *unmodified* mainline fs/ubifs/scan.c is compiled against a small
# user-space stub of ubifs.h, so ubifs_scan_a_node() can be regression
# tested without flash hardware, an MTD simulator, or a VM.
#
# Usage:
#   make        # build the test binary (build/ubifs_ut)
#   make test   # build (if needed) and run the unit tests
#   make verify # run tests + mutation checks (proves the tests catch regressions)
#   make clean

CC      ?= cc
# -Wno-unused-parameter / -Wno-sign-compare: the kernel's scan.c (compiled
# verbatim, must stay byte-identical to fs/ubifs/scan.c) triggers these
# under -Wextra; they are harmless for a user-space harness.
CFLAGS  ?= -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -g
CPPFLAGS += -Iinclude -Itests
BUILD   := build
BIN     := $(BUILD)/ubifs_ut

OBJS := $(BUILD)/scan.o $(BUILD)/ut_support.o $(BUILD)/test_scan_a_node.o

# 'make' alone builds the binary; 'make test' is the entry most people want.
.DEFAULT_GOAL := all
all: $(BIN)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/scan.o: src/scan.c include/ubifs.h | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/ut_support.o: tests/ut_support.c tests/ut_support.h include/ubifs.h | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD)/test_scan_a_node.o: tests/test_scan_a_node.c tests/ut_support.h include/ubifs.h | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

test: $(BIN)
	./$(BIN) && echo "PASS: all tests passed" || { echo "FAIL: tests failed"; exit 1; }

# Mutation testing: inject three typical regressions into a copy of the
# code under test and check that the test suite catches every one of them.
verify: $(BIN) test
	@fail=0; \
	for m in 's/(node_len + pad_len) & 7/(node_len + pad_len) \& 3/' \
	         's/offs + node_len + pad_len > c->leb_size/offs + node_len + pad_len >= c->leb_size/' \
	         's/\*p++ == UBIFS_PADDING_BYTE/*p++ != UBIFS_PADDING_BYTE/'; do \
		sed "$$m" src/scan.c > $(BUILD)/scan_mutant.c; \
		if $(CC) $(CFLAGS) $(CPPFLAGS) $(BUILD)/scan_mutant.c tests/ut_support.c \
		    tests/test_scan_a_node.c -o $(BUILD)/mutant 2>/dev/null \
		    && ./$(BUILD)/mutant >/dev/null 2>&1; then \
			echo "MUTANT NOT CAUGHT: $$m"; fail=1; \
		else \
			echo "mutant caught: $$m"; \
		fi; \
	done; \
	[ $$fail -eq 0 ] && echo "PASS: all mutants caught by the test suite" \
			 || { echo "FAIL: at least one mutant survived"; exit 1; }

clean:
	rm -rf $(BUILD)

.PHONY: all test verify clean
