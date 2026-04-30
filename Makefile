CC ?= cc
BISON ?= bison

CFLAGS ?= -std=c99 -O2 -g
CPPFLAGS := -Iinclude -Isrc -Ibuild
WARNFLAGS := -Wall -Wextra -Werror
PARSER_WARNFLAGS := -Wall -Wextra -Wno-unused-but-set-variable

BUILD_DIR := build
BIN_DIR := bin

PARSER_C := $(BUILD_DIR)/parser_bison.c
PARSER_H := $(BUILD_DIR)/parser_bison.h

LIB_OBJS := \
	$(BUILD_DIR)/parser_bison.o \
	$(BUILD_DIR)/lexer.o \
	$(BUILD_DIR)/parser_api.o

.PHONY: all clean test smoke corpus

all: $(BIN_DIR)/mylite-parse

$(PARSER_C) $(PARSER_H): src/parser.y | $(BUILD_DIR)
	$(BISON) -d -o $(PARSER_C) src/parser.y

$(BUILD_DIR)/parser_bison.o: $(PARSER_C) $(PARSER_H) include/mylite/parser.h src/parser_internal.h src/lexer.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(PARSER_WARNFLAGS) -c $(PARSER_C) -o $@

$(BUILD_DIR)/lexer.o: src/lexer.c $(PARSER_H) include/mylite/parser.h src/parser_internal.h src/lexer.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(WARNFLAGS) -c src/lexer.c -o $@

$(BUILD_DIR)/parser_api.o: src/parser_api.c $(PARSER_H) include/mylite/parser.h src/parser_internal.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(WARNFLAGS) -c src/parser_api.c -o $@

$(BUILD_DIR)/main.o: src/main.c include/mylite/parser.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(WARNFLAGS) -c src/main.c -o $@

$(BIN_DIR)/mylite-parse: $(LIB_OBJS) $(BUILD_DIR)/main.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LIB_OBJS) $(BUILD_DIR)/main.o -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

test: smoke

smoke: $(BIN_DIR)/mylite-parse
	tests/run-smoke.sh

corpus: $(BIN_DIR)/mylite-parse
	python3 tests/parse_corpus.py --parser $(BIN_DIR)/mylite-parse --download

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
