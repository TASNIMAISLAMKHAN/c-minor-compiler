.SUFFIXES:
CC = gcc
FLEX = flex
BISON = bison

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Iinclude -Ibuild/generated
LDFLAGS =

BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
GEN_DIR = $(BUILD_DIR)/generated
BIN_DIR = $(BUILD_DIR)/bin

LEXER_SOURCE = $(GEN_DIR)/lexer_generated.c
PARSER_SOURCE = $(GEN_DIR)/parser.tab.c
PARSER_HEADER = $(GEN_DIR)/parser.tab.h

COMMON_SOURCES = \
	src/common/diagnostic.c \
	src/lexer/token.c \
	src/lexer/lexer_support.c \
	src/lexer/lexer.c \
	$(LEXER_SOURCE) \
	src/ast/ast.c \
	$(PARSER_SOURCE) \
	src/parser/parser.c \
	src/semantic/semantic.c \
	src/ir/ir.c \
	src/backend/llvm_backend.c

COMMON_OBJECTS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(COMMON_SOURCES))

TESTS = \
	$(BIN_DIR)/lexer_test \
	$(BIN_DIR)/parser_test \
	$(BIN_DIR)/semantic_test \
	$(BIN_DIR)/ir_test \
	$(BIN_DIR)/backend_test

.PHONY: all clean test directories

all: directories $(BIN_DIR)/cminor

directories:
	mkdir -p $(OBJ_DIR)/src/common
	mkdir -p $(OBJ_DIR)/src/lexer
	mkdir -p $(OBJ_DIR)/src/ast
	mkdir -p $(OBJ_DIR)/src/parser
	mkdir -p $(OBJ_DIR)/src/semantic
	mkdir -p $(OBJ_DIR)/src/ir
	mkdir -p $(OBJ_DIR)/src/backend
	mkdir -p $(OBJ_DIR)/tests
	mkdir -p $(GEN_DIR)
	mkdir -p $(BIN_DIR)

$(LEXER_SOURCE): src/lexer/lexer.l | directories
	$(FLEX) -o $@ $<

$(PARSER_SOURCE) $(PARSER_HEADER) &: src/parser/parser.y | directories
	$(BISON) -Wall -Wcounterexamples -d \
		-o $(PARSER_SOURCE) \
		--defines=$(PARSER_HEADER) $<

$(OBJ_DIR)/%.o: %.c $(PARSER_HEADER) $(LEXER_SOURCE)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/cminor: src/main.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/lexer_test: tests/lexer_smoke_test.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/parser_test: tests/parser_test.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/semantic_test: tests/semantic_test.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/ir_test: tests/ir_test.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/backend_test: tests/backend_test.c $(COMMON_OBJECTS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all $(TESTS)
	@echo "========== LEXER TEST =========="
	./$(BIN_DIR)/lexer_test examples/lexer_smoke.cm
	@echo "========== PARSER TEST =========="
	./$(BIN_DIR)/parser_test examples/semantic_valid.cm
	@echo "========== SEMANTIC TEST =========="
	./$(BIN_DIR)/semantic_test examples/semantic_valid.cm
	@echo "========== IR TEST =========="
	./$(BIN_DIR)/ir_test examples/ir_control_flow.cm
	@echo "========== BACKEND TEST =========="
	./$(BIN_DIR)/backend_test examples/semantic_valid.cm $(BUILD_DIR)/make_test.ll
	llc -filetype=obj $(BUILD_DIR)/make_test.ll -o $(BUILD_DIR)/make_test.o
	@echo "========== ALL TESTS PASSED =========="

clean:
	rm -rf $(BUILD_DIR)
