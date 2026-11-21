#ifndef CMINOR_IR_H
#define CMINOR_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "cminor/ast/ast.h"

typedef enum {
    IR_FUNCTION_BEGIN,
    IR_PARAMETER,
    IR_FUNCTION_END,

    IR_CONSTANT,
    IR_LOAD,
    IR_STORE,
    IR_MOVE,

    IR_ADD,
    IR_SUBTRACT,
    IR_MULTIPLY,
    IR_DIVIDE,
    IR_MODULO,
    IR_NEGATE,

    IR_EQUAL,
    IR_NOT_EQUAL,
    IR_LESS,
    IR_LESS_EQUAL,
    IR_GREATER,
    IR_GREATER_EQUAL,
    IR_BOOL_NOT,

    IR_ARGUMENT,
    IR_CALL,

    IR_LABEL,
    IR_JUMP,
    IR_BRANCH_ZERO,
    IR_BRANCH_NONZERO,

    IR_RETURN
} IROpcode;

typedef struct {
    IROpcode opcode;

    int result;
    int operand1;
    int operand2;

    int32_t immediate;

    unsigned int symbol_id;
    unsigned int label;
    unsigned int argument_index;

    size_t argument_count;
    char *name;
} IRInstruction;

typedef struct {
    IRInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;

    int next_temporary;
    unsigned int next_label;

    bool valid;
} IRProgram;

void ir_program_init(IRProgram *program);

int ir_generate(const AstNode *ast, IRProgram *program);

void ir_dump(const IRProgram *program, FILE *output);

void ir_program_destroy(IRProgram *program);

#endif
