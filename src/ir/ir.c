#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cminor/ir/ir.h"
#include "cminor/semantic/semantic.h"
#include "cminor/lexer/token.h"

typedef struct {
    unsigned int break_label;
    unsigned int continue_label;
} LoopTarget;

typedef struct {
    IRProgram *program;
    LoopTarget loops[256];
    size_t loop_depth;
} IRContext;

/* ---------------------------------------------------------
   Memory and instruction helpers
   --------------------------------------------------------- */

static char *copy_string(const char *text)
{
    if (text == NULL) {
        return NULL;
    }

    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, text, length);
    }

    return copy;
}

void ir_program_init(IRProgram *program)
{
    if (program == NULL) {
        return;
    }

    memset(program, 0, sizeof(*program));
    program->next_temporary = 1;
    program->next_label = 1;
    program->valid = true;
}

static IRInstruction *new_instruction(
    IRProgram *program,
    IROpcode opcode
)
{
    if (program == NULL || !program->valid) {
        return NULL;
    }

    if (program->instruction_count ==
        program->instruction_capacity) {

        size_t new_capacity =
            program->instruction_capacity == 0
                ? 64
                : program->instruction_capacity * 2;

        IRInstruction *items = realloc(
            program->instructions,
            new_capacity * sizeof(*items)
        );

        if (items == NULL) {
            program->valid = false;
            return NULL;
        }

        program->instructions = items;
        program->instruction_capacity = new_capacity;
    }

    IRInstruction *instruction =
        &program->instructions[program->instruction_count++];

    memset(instruction, 0, sizeof(*instruction));
    instruction->opcode = opcode;

    return instruction;
}

static int new_temporary(IRContext *ctx)
{
    return ctx->program->next_temporary++;
}

static unsigned int new_label(IRContext *ctx)
{
    return ctx->program->next_label++;
}

static void emit_simple(
    IRContext *ctx,
    IROpcode opcode,
    int result,
    int operand1,
    int operand2
)
{
    IRInstruction *instruction =
        new_instruction(ctx->program, opcode);

    if (instruction == NULL) {
        return;
    }

    instruction->result = result;
    instruction->operand1 = operand1;
    instruction->operand2 = operand2;
}

static void emit_named(
    IRContext *ctx,
    IROpcode opcode,
    const char *name,
    unsigned int symbol_id
)
{
    IRInstruction *instruction =
        new_instruction(ctx->program, opcode);

    if (instruction == NULL) {
        return;
    }

    instruction->name = copy_string(name);
    instruction->symbol_id = symbol_id;

    if (name != NULL && instruction->name == NULL) {
        ctx->program->valid = false;
    }
}

static void emit_label(IRContext *ctx, unsigned int label)
{
    IRInstruction *instruction =
        new_instruction(ctx->program, IR_LABEL);

    if (instruction != NULL) {
        instruction->label = label;
    }
}

static void emit_jump(
    IRContext *ctx,
    IROpcode opcode,
    unsigned int label,
    int condition
)
{
    IRInstruction *instruction =
        new_instruction(ctx->program, opcode);

    if (instruction == NULL) {
        return;
    }

    instruction->label = label;
    instruction->operand1 = condition;
}

static void emit_constant(
    IRContext *ctx,
    int result,
    int32_t value
)
{
    IRInstruction *instruction =
        new_instruction(ctx->program, IR_CONSTANT);

    if (instruction == NULL) {
        return;
    }

    instruction->result = result;
    instruction->immediate = value;
}

static void emit_symbol(
    IRContext *ctx,
    IROpcode opcode,
    int value,
    const AstNode *node
)
{
    if (node == NULL || node->symbol == NULL) {
        ctx->program->valid = false;
        return;
    }

    IRInstruction *instruction =
        new_instruction(ctx->program, opcode);

    if (instruction == NULL) {
        return;
    }

    instruction->symbol_id = node->symbol->id;
    instruction->name = copy_string(node->symbol->name);

    if (instruction->name == NULL) {
        ctx->program->valid = false;
    }

    if (opcode == IR_LOAD) {
        instruction->result = value;
    } else {
        instruction->operand1 = value;
    }
}

/* ---------------------------------------------------------
   Forward declarations
   --------------------------------------------------------- */

static int generate_expression(IRContext *ctx, const AstNode *node);

static void generate_statement(
    IRContext *ctx,
    const AstNode *node
);

/* ---------------------------------------------------------
   Expression generation
   --------------------------------------------------------- */

/*
 * Logical expressions use explicit control flow.
 * This implementation emits a temporary assignment
 * on both paths.
 */
static int generate_logical_value(
    IRContext *ctx,
    const AstNode *node
)
{
    int result = new_temporary(ctx);

    unsigned int true_label = new_label(ctx);
    unsigned int false_label = new_label(ctx);
    unsigned int end_label = new_label(ctx);

    int left = generate_expression(ctx, node->first);

    if (node->operation == TOKEN_LOGICAL_AND) {
        emit_jump(ctx, IR_BRANCH_ZERO, false_label, left);

        int right = generate_expression(ctx, node->second);
        emit_jump(ctx, IR_BRANCH_ZERO, false_label, right);
        emit_jump(ctx, IR_JUMP, true_label, 0);
    } else {
        emit_jump(ctx, IR_BRANCH_NONZERO, true_label, left);

        int right = generate_expression(ctx, node->second);
        emit_jump(ctx, IR_BRANCH_NONZERO, true_label, right);
        emit_jump(ctx, IR_JUMP, false_label, 0);
    }

    emit_label(ctx, true_label);
    emit_constant(ctx, result, 1);
    emit_jump(ctx, IR_JUMP, end_label, 0);

    emit_label(ctx, false_label);
    emit_constant(ctx, result, 0);

    emit_label(ctx, end_label);

    return result;
}

static int generate_call(
    IRContext *ctx,
    const AstNode *node
)
{
    size_t argument_count = 0;

    for (const AstNode *argument = node->children;
         argument != NULL;
         argument = argument->next) {
        argument_count++;
    }

    int *arguments = NULL;

    if (argument_count > 0) {
        arguments = malloc(argument_count * sizeof(*arguments));

        if (arguments == NULL) {
            ctx->program->valid = false;
            return -1;
        }
    }

    size_t index = 0;

    /*
     * Evaluate every argument first.
     * This allows nested calls without interrupting
     * the outer call's ARG instruction sequence.
     */
    for (const AstNode *argument = node->children;
         argument != NULL;
         argument = argument->next) {

        arguments[index++] =
            generate_expression(ctx, argument);

        if (!ctx->program->valid) {
            free(arguments);
            return -1;
        }
    }

    for (size_t i = 0; i < argument_count; i++) {
        IRInstruction *instruction =
            new_instruction(ctx->program, IR_ARGUMENT);

        if (instruction == NULL) {
            free(arguments);
            return -1;
        }

        instruction->operand1 = arguments[i];
        instruction->argument_index = (unsigned int)i;
    }

    free(arguments);

    int result = new_temporary(ctx);

    IRInstruction *instruction =
        new_instruction(ctx->program, IR_CALL);

    if (instruction == NULL) {
        return -1;
    }

    instruction->result = result;
    instruction->argument_count = argument_count;

    if (node->symbol == NULL) {
        ctx->program->valid = false;
        return -1;
    }

    instruction->symbol_id = node->symbol->id;
    instruction->name = copy_string(node->symbol->name);

    if (instruction->name == NULL) {
        ctx->program->valid = false;
        return -1;
    }

    return result;
}

static int generate_expression(
    IRContext *ctx,
    const AstNode *node
)
{
    if (node == NULL) {
        ctx->program->valid = false;
        return -1;
    }

    switch (node->kind) {
        case AST_INTEGER: {
            int result = new_temporary(ctx);
            emit_constant(ctx, result, node->integer_value);
            return result;
        }

        case AST_IDENTIFIER: {
            int result = new_temporary(ctx);
            emit_symbol(ctx, IR_LOAD, result, node);
            return result;
        }

        case AST_GROUP:
            return generate_expression(ctx, node->first);

        case AST_CALL:
            return generate_call(ctx, node);

        case AST_ASSIGNMENT: {
            if (node->first == NULL ||
                node->first->kind != AST_IDENTIFIER) {
                ctx->program->valid = false;
                return -1;
            }

            int value = generate_expression(ctx, node->second);

            emit_symbol(
                ctx,
                IR_STORE,
                value,
                node->first
            );

            return value;
        }

        case AST_UNARY: {
            int operand =
                generate_expression(ctx, node->first);

            if (node->operation == TOKEN_PLUS) {
                return operand;
            }

            int result = new_temporary(ctx);

            if (node->operation == TOKEN_MINUS) {
                emit_simple(
                    ctx,
                    IR_NEGATE,
                    result,
                    operand,
                    0
                );
            } else if (node->operation == TOKEN_NOT) {
                emit_simple(
                    ctx,
                    IR_BOOL_NOT,
                    result,
                    operand,
                    0
                );
            } else {
                ctx->program->valid = false;
                return -1;
            }

            return result;
        }

        case AST_BINARY: {
            if (node->operation == TOKEN_LOGICAL_AND ||
                node->operation == TOKEN_LOGICAL_OR) {
                return generate_logical_value(ctx, node);
            }

            int left =
                generate_expression(ctx, node->first);

            int right =
                generate_expression(ctx, node->second);

            int result = new_temporary(ctx);
            IROpcode opcode;

            switch (node->operation) {
                case TOKEN_PLUS: opcode = IR_ADD; break;
                case TOKEN_MINUS: opcode = IR_SUBTRACT; break;
                case TOKEN_MULTIPLY: opcode = IR_MULTIPLY; break;
                case TOKEN_DIVIDE: opcode = IR_DIVIDE; break;
                case TOKEN_MODULO: opcode = IR_MODULO; break;

                case TOKEN_EQUAL: opcode = IR_EQUAL; break;
                case TOKEN_NOT_EQUAL: opcode = IR_NOT_EQUAL; break;
                case TOKEN_LESS: opcode = IR_LESS; break;
                case TOKEN_LESS_EQUAL: opcode = IR_LESS_EQUAL; break;
                case TOKEN_GREATER: opcode = IR_GREATER; break;
                case TOKEN_GREATER_EQUAL: opcode = IR_GREATER_EQUAL; break;

                default:
                    ctx->program->valid = false;
                    return -1;
            }

            emit_simple(
                ctx,
                opcode,
                result,
                left,
                right
            );

            return result;
        }

        default:
            ctx->program->valid = false;
            return -1;
    }
}

/* ---------------------------------------------------------
   Statement generation
   --------------------------------------------------------- */

static void generate_block(
    IRContext *ctx,
    const AstNode *block
)
{
    for (const AstNode *item = block->children;
         item != NULL;
         item = item->next) {
        generate_statement(ctx, item);
    }
}

static void generate_if(
    IRContext *ctx,
    const AstNode *node
)
{
    unsigned int else_label = new_label(ctx);
    unsigned int end_label = new_label(ctx);

    int condition = generate_expression(ctx, node->first);

    emit_jump(
        ctx,
        IR_BRANCH_ZERO,
        else_label,
        condition
    );

    generate_statement(ctx, node->second);

    if (node->third != NULL) {
        emit_jump(ctx, IR_JUMP, end_label, 0);
    }

    emit_label(ctx, else_label);

    if (node->third != NULL) {
        generate_statement(ctx, node->third);
        emit_label(ctx, end_label);
    }
}

static void push_loop(
    IRContext *ctx,
    unsigned int break_label,
    unsigned int continue_label
)
{
    if (ctx->loop_depth >= 256) {
        ctx->program->valid = false;
        return;
    }

    ctx->loops[ctx->loop_depth++] = (LoopTarget){
        break_label,
        continue_label
    };
}

static void generate_while(
    IRContext *ctx,
    const AstNode *node
)
{
    unsigned int condition_label = new_label(ctx);
    unsigned int body_label = new_label(ctx);
    unsigned int end_label = new_label(ctx);

    emit_label(ctx, condition_label);

    int condition = generate_expression(ctx, node->first);

    emit_jump(
        ctx,
        IR_BRANCH_ZERO,
        end_label,
        condition
    );

    emit_label(ctx, body_label);

    push_loop(ctx, end_label, condition_label);
    generate_statement(ctx, node->second);

    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
    }

    emit_jump(ctx, IR_JUMP, condition_label, 0);
    emit_label(ctx, end_label);
}

static void generate_for(
    IRContext *ctx,
    const AstNode *node
)
{
    unsigned int condition_label = new_label(ctx);
    unsigned int update_label = new_label(ctx);
    unsigned int end_label = new_label(ctx);

    if (node->first != NULL) {
        generate_statement(ctx, node->first);
    }

    emit_label(ctx, condition_label);

    if (node->second != NULL) {
        int condition =
            generate_expression(ctx, node->second);

        emit_jump(
            ctx,
            IR_BRANCH_ZERO,
            end_label,
            condition
        );
    }

    push_loop(ctx, end_label, update_label);
    generate_statement(ctx, node->children);

    if (ctx->loop_depth > 0) {
        ctx->loop_depth--;
    }

    emit_label(ctx, update_label);

    if (node->third != NULL) {
        (void)generate_expression(ctx, node->third);
    }

    emit_jump(ctx, IR_JUMP, condition_label, 0);
    emit_label(ctx, end_label);
}

static void generate_statement(
    IRContext *ctx,
    const AstNode *node
)
{
    if (node == NULL || !ctx->program->valid) {
        return;
    }

    switch (node->kind) {
        case AST_BLOCK:
            generate_block(ctx, node);
            break;

        case AST_VARIABLE_DECLARATION:
            if (node->first != NULL) {
                int value =
                    generate_expression(ctx, node->first);

                emit_symbol(
                    ctx,
                    IR_STORE,
                    value,
                    node
                );
            }
            break;

        case AST_EXPRESSION_STATEMENT:
            (void)generate_expression(ctx, node->first);
            break;

        case AST_EMPTY_STATEMENT:
            break;

        case AST_IF_STATEMENT:
            generate_if(ctx, node);
            break;

        case AST_WHILE_STATEMENT:
            generate_while(ctx, node);
            break;

        case AST_FOR_STATEMENT:
            generate_for(ctx, node);
            break;

        case AST_BREAK_STATEMENT:
            if (ctx->loop_depth == 0) {
                ctx->program->valid = false;
                break;
            }

            emit_jump(
                ctx,
                IR_JUMP,
                ctx->loops[ctx->loop_depth - 1].break_label,
                0
            );
            break;

        case AST_CONTINUE_STATEMENT:
            if (ctx->loop_depth == 0) {
                ctx->program->valid = false;
                break;
            }

            emit_jump(
                ctx,
                IR_JUMP,
                ctx->loops[ctx->loop_depth - 1].continue_label,
                0
            );
            break;

        case AST_RETURN_STATEMENT: {
            int value = -1;

            if (node->first != NULL) {
                value = generate_expression(ctx, node->first);
            }

            emit_simple(ctx, IR_RETURN, -1, value, 0);
            break;
        }

        default:
            ctx->program->valid = false;
            break;
    }
}

/* ---------------------------------------------------------
   Public IR generation interface
   --------------------------------------------------------- */

int ir_generate(const AstNode *ast, IRProgram *program)
{
    if (ast == NULL || program == NULL ||
        ast->kind != AST_PROGRAM) {
        return 0;
    }

    ir_program_init(program);

    IRContext ctx = {0};
    ctx.program = program;

    for (const AstNode *function = ast->children;
         function != NULL && program->valid;
         function = function->next) {

        if (function->kind != AST_FUNCTION) {
            program->valid = false;
            break;
        }

        /* Prototypes do not generate executable IR. */
        if (!function->is_definition) {
            continue;
        }

        if (function->symbol == NULL) {
            program->valid = false;
            break;
        }

        emit_named(
            &ctx,
            IR_FUNCTION_BEGIN,
            function->name,
            function->symbol->id
        );

        for (const AstNode *parameter = function->first;
             parameter != NULL;
             parameter = parameter->next) {

            emit_symbol(
                &ctx,
                IR_PARAMETER,
                -1,
                parameter
            );
        }

        generate_statement(&ctx, function->second);

        emit_named(
            &ctx,
            IR_FUNCTION_END,
            function->name,
            function->symbol->id
        );
    }

    return program->valid ? 1 : 0;
}

/* ---------------------------------------------------------
   IR dump
   --------------------------------------------------------- */

static const char *opcode_name(IROpcode opcode)
{
    switch (opcode) {
        case IR_FUNCTION_BEGIN: return "FUNCTION_BEGIN";
        case IR_PARAMETER: return "PARAM";
        case IR_FUNCTION_END: return "FUNCTION_END";

        case IR_CONSTANT: return "CONST";
        case IR_LOAD: return "LOAD";
        case IR_STORE: return "STORE";
        case IR_MOVE: return "MOVE";

        case IR_ADD: return "ADD";
        case IR_SUBTRACT: return "SUB";
        case IR_MULTIPLY: return "MUL";
        case IR_DIVIDE: return "DIV";
        case IR_MODULO: return "MOD";
        case IR_NEGATE: return "NEG";

        case IR_EQUAL: return "EQ";
        case IR_NOT_EQUAL: return "NE";
        case IR_LESS: return "LT";
        case IR_LESS_EQUAL: return "LE";
        case IR_GREATER: return "GT";
        case IR_GREATER_EQUAL: return "GE";
        case IR_BOOL_NOT: return "BOOL_NOT";

        case IR_ARGUMENT: return "ARG";
        case IR_CALL: return "CALL";

        case IR_LABEL: return "LABEL";
        case IR_JUMP: return "JUMP";
        case IR_BRANCH_ZERO: return "BR_ZERO";
        case IR_BRANCH_NONZERO: return "BR_NONZERO";

        case IR_RETURN: return "RETURN";
        default: return "UNKNOWN";
    }
}

void ir_dump(const IRProgram *program, FILE *output)
{
    if (program == NULL || output == NULL) {
        return;
    }

    fprintf(output, "========== STUDENT IR ==========\n");

    for (size_t i = 0; i < program->instruction_count; i++) {
        const IRInstruction *instruction =
            &program->instructions[i];

        fprintf(
            output,
            "%04zu: ",
            i
        );

        switch (instruction->opcode) {
            case IR_FUNCTION_BEGIN:
            case IR_FUNCTION_END:
                fprintf(
                    output,
                    "%s %s [symbol=%u]\n",
                    opcode_name(instruction->opcode),
                    instruction->name,
                    instruction->symbol_id
                );
                break;

            case IR_PARAMETER:
                fprintf(
                    output,
                    "PARAM %s [symbol=%u]\n",
                    instruction->name,
                    instruction->symbol_id
                );
                break;

            case IR_CONSTANT:
                fprintf(
                    output,
                    "t%d = CONST %d\n",
                    instruction->result,
                    instruction->immediate
                );
                break;

            case IR_LOAD:
                fprintf(
                    output,
                    "t%d = LOAD %s [symbol=%u]\n",
                    instruction->result,
                    instruction->name,
                    instruction->symbol_id
                );
                break;

            case IR_STORE:
                fprintf(
                    output,
                    "STORE %s [symbol=%u], t%d\n",
                    instruction->name,
                    instruction->symbol_id,
                    instruction->operand1
                );
                break;

            case IR_MOVE:
                fprintf(
                    output,
                    "t%d = MOVE t%d\n",
                    instruction->result,
                    instruction->operand1
                );
                break;

            case IR_NEGATE:
            case IR_BOOL_NOT:
                fprintf(
                    output,
                    "t%d = %s t%d\n",
                    instruction->result,
                    opcode_name(instruction->opcode),
                    instruction->operand1
                );
                break;

            case IR_ADD:
            case IR_SUBTRACT:
            case IR_MULTIPLY:
            case IR_DIVIDE:
            case IR_MODULO:
            case IR_EQUAL:
            case IR_NOT_EQUAL:
            case IR_LESS:
            case IR_LESS_EQUAL:
            case IR_GREATER:
            case IR_GREATER_EQUAL:
                fprintf(
                    output,
                    "t%d = %s t%d, t%d\n",
                    instruction->result,
                    opcode_name(instruction->opcode),
                    instruction->operand1,
                    instruction->operand2
                );
                break;

            case IR_ARGUMENT:
                fprintf(
                    output,
                    "ARG %u, t%d\n",
                    instruction->argument_index,
                    instruction->operand1
                );
                break;

            case IR_CALL:
                fprintf(
                    output,
                    "t%d = CALL %s [symbol=%u, argc=%zu]\n",
                    instruction->result,
                    instruction->name,
                    instruction->symbol_id,
                    instruction->argument_count
                );
                break;

            case IR_LABEL:
                fprintf(output, "L%u:\n", instruction->label);
                break;

            case IR_JUMP:
                fprintf(
                    output,
                    "JUMP L%u\n",
                    instruction->label
                );
                break;

            case IR_BRANCH_ZERO:
            case IR_BRANCH_NONZERO:
                fprintf(
                    output,
                    "%s t%d, L%u\n",
                    opcode_name(instruction->opcode),
                    instruction->operand1,
                    instruction->label
                );
                break;

            case IR_RETURN:
                if (instruction->operand1 >= 0) {
                    fprintf(
                        output,
                        "RETURN t%d\n",
                        instruction->operand1
                    );
                } else {
                    fprintf(output, "RETURN\n");
                }
                break;

            default:
                fprintf(
                    output,
                    "%s\n",
                    opcode_name(instruction->opcode)
                );
                break;
        }
    }

    fprintf(output, "================================\n");
}

void ir_program_destroy(IRProgram *program)
{
    if (program == NULL) {
        return;
    }

    for (size_t i = 0; i < program->instruction_count; i++) {
        free(program->instructions[i].name);
    }

    free(program->instructions);
    memset(program, 0, sizeof(*program));
}
