CC ?= cc

BUILD_DIR := build
LEMON := $(BUILD_DIR)/lemon
PARSER_GEN_DIR := src/parser/generated
PARSER_GRAMMAR := src/parser/mylite_lemon.y
PARSER_GEN_C := $(PARSER_GEN_DIR)/mylite_lemon.c
PARSER_GEN_H := $(PARSER_GEN_DIR)/mylite_lemon.h

CPPFLAGS := -Isrc/parser
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Werror
LEMON_CFLAGS := -std=c99 -O2

PARSER_SOURCES := \
	src/parser/mylite_lexer.c \
	src/parser/mylite_parser.c \
	$(PARSER_GEN_C)

CLI_SOURCES := tools/mylite_parse.c
OBJECTS := \
	$(BUILD_DIR)/mylite_lexer.o \
	$(BUILD_DIR)/mylite_parser.o \
	$(BUILD_DIR)/mylite_lemon.o \
	$(BUILD_DIR)/mylite_parse.o

.PHONY: all clean regen-parser test-parser test-parser-strict

all: $(BUILD_DIR)/mylite-parse

$(BUILD_DIR)/mylite-parse: $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $@

$(BUILD_DIR)/mylite_lexer.o: src/parser/mylite_lexer.c $(PARSER_GEN_H)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mylite_parser.o: src/parser/mylite_parser.c $(PARSER_GEN_H)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mylite_lemon.o: $(PARSER_GEN_C) $(PARSER_GEN_H)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mylite_parse.o: tools/mylite_parse.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LEMON): tools/lemon.c
	@mkdir -p $(dir $@)
	$(CC) $(LEMON_CFLAGS) $< -o $@

$(PARSER_GEN_C) $(PARSER_GEN_H): $(PARSER_GRAMMAR) $(LEMON) tools/lempar.c
	@mkdir -p $(PARSER_GEN_DIR)
	$(LEMON) -Ttools/lempar.c -d$(PARSER_GEN_DIR) $(PARSER_GRAMMAR)

regen-parser: $(PARSER_GEN_C) $(PARSER_GEN_H)

test-parser: all
	python3 scripts/test_parser_smoke.py --exe $(BUILD_DIR)/mylite-parse
	python3 scripts/test_parser_corpus.py --exe $(BUILD_DIR)/mylite-parse
	python3 scripts/test_parser_corpus_strict.py --exe $(BUILD_DIR)/mylite-parse

test-parser-strict: all
	python3 scripts/test_parser_corpus_strict.py --exe $(BUILD_DIR)/mylite-parse

clean:
	rm -rf $(BUILD_DIR)
