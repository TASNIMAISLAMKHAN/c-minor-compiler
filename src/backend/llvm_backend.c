#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cminor/backend/llvm_backend.h"

typedef struct {
    const IRProgram *program;
    const SemanticModel *semantic;
    FILE *output;

    unsigned int max_symbol;
    bool *used_symbols;

    size_t pending_arguments[8];
    size_t pending_count;

    bool block_terminated;
    bool valid;
} LLVMContext;

static const SemanticSymbol *find_function(
    const SemanticModel *model,
    unsigned int id
)
{
    if (model == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < model->symbol_count; i++) {
        const SemanticSymbol *symbol = model->symbols[i];

        if (symbol != NULL &&
            symbol->id == id &&
            symbol->kind == SYMBOL_FUNCTION) {
            return symbol;
        }
    }

    return NULL;
}

static void mark_symbol(
    LLVMContext *ctx,
    unsigned int id
)
{
    if (id == 0 || id > ctx->max_symbol) {
        ctx->valid = false;
        return;
    }

    ctx->used_symbols[id] = true;
}

static void scan_function_symbols(
    LLVMContext *ctx,
    size_t begin,
    size_t end
)
{
    memset(
        ctx->used_symbols,
        0,
        ((size_t)ctx->max_symbol + 1) * sizeof(bool)
    );

    for (size_t i = begin; i < end; i++) {
        const IRInstruction *instruction =
            &ctx->program->instructions[i];

        switch (instruction->opcode) {
            case IR_PARAMETER:
            case IR_LOAD:
            case IR_STORE:
                mark_symbol(ctx, instruction->symbol_id);
                break;

            default:
                break;
        }
    }
}

static void emit_allocations(
    LLVMContext *ctx,
    size_t begin,
    size_t end
)
{
    fprintf(ctx->output, "entry:\n");

    for (unsigned int id = 1; id <= ctx->max_symbol; id++) {
        if (ctx->used_symbols[id]) {
            fprintf(
                ctx->output,
                "  %%v%u = alloca i32, align 4\n",
                id
            );
        }
    }

    int temporary_count = ctx->program->next_temporary;

    for (int id = 1; id < temporary_count; id++) {
        fprintf(
            ctx->output,
            "  %%t%d = alloca i32, align 4\n",
            id
        );
    }

    (void)begin;
    (void)end;
}

static void emit_load_temporary(
    LLVMContext *ctx,
    int temporary,
    size_t instruction_index,
    char *name,
    size_t name_size
)
{
    if (temporary <= 0) {
        ctx->valid = false;
        return;
    }

    snprintf(
        name,
        name_size,
        "%%load%zu",
        instruction_index
    );

    fprintf(
        ctx->output,
        "  %s = load i32, ptr %%t%d, align 4\n",
        name,
        temporary
    );
}

static void emit_store_temporary(
    LLVMContext *ctx,
    int temporary,
    const char *value
)
{
    if (temporary <= 0) {
        ctx->valid = false;
        return;
    }

    fprintf(
        ctx->output,
        "  store i32 %s, ptr %%t%d, align 4\n",
        value,
        temporary
    );
}

static void emit_store_symbol(
    LLVMContext *ctx,
    unsigned int symbol,
    const char *value
)
{
    if (symbol == 0 || symbol > ctx->max_symbol) {
        ctx->valid = false;
        return;
    }

    fprintf(
        ctx->output,
        "  store i32 %s, ptr %%v%u, align 4\n",
        value,
        symbol
    );
}

static void emit_load_symbol(
    LLVMContext *ctx,
    unsigned int symbol,
    size_t index,
    char *name,
    size_t name_size
)
{
    if (symbol == 0 || symbol > ctx->max_symbol) {
        ctx->valid = false;
        return;
    }

    snprintf(name, name_size, "%%var%zu", index);

    fprintf(
        ctx->output,
        "  %s = load i32, ptr %%v%u, align 4\n",
        name,
        symbol
    );
}

static void emit_instruction(
    LLVMContext *ctx,
    const IRInstruction *instruction,
    size_t index,
    unsigned int *parameter_index
)
{
    char left[64];
    char right[64];
    char result[64];

    if (!ctx->valid) {
        return;
    }

    /*
     * A terminated block cannot contain another instruction.
     * A new IR_LABEL starts a fresh basic block.
     */
    if (ctx->block_terminated &&
        instruction->opcode != IR_LABEL &&
        instruction->opcode != IR_FUNCTION_END) {
        return;
    }

    switch (instruction->opcode) {
        case IR_FUNCTION_BEGIN:
        case IR_FUNCTION_END:
            break;

        case IR_PARAMETER:
            snprintf(
                result,
                sizeof(result),
                "%%arg%u",
                (*parameter_index)++
            );

            emit_store_symbol(
                ctx,
                instruction->symbol_id,
                result
            );
            break;

        case IR_CONSTANT:
            snprintf(
                result,
                sizeof(result),
                "%d",
                instruction->immediate
            );

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;

        case IR_LOAD:
            emit_load_symbol(
                ctx,
                instruction->symbol_id,
                index,
                result,
                sizeof(result)
            );

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;

        case IR_STORE:
            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                result,
                sizeof(result)
            );

            emit_store_symbol(
                ctx,
                instruction->symbol_id,
                result
            );
            break;

        case IR_MOVE:
            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                result,
                sizeof(result)
            );

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;

        case IR_NEGATE:
            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                left,
                sizeof(left)
            );

            fprintf(
                ctx->output,
                "  %%neg%zu = sub i32 0, %s\n",
                index,
                left
            );

            snprintf(result, sizeof(result), "%%neg%zu", index);

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;

        case IR_BOOL_NOT:
            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                left,
                sizeof(left)
            );

            fprintf(
                ctx->output,
                "  %%cmp%zu = icmp eq i32 %s, 0\n",
                index,
                left
            );

            fprintf(
                ctx->output,
                "  %%bool%zu = zext i1 %%cmp%zu to i32\n",
                index,
                index
            );

            snprintf(result, sizeof(result), "%%bool%zu", index);

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;

        case IR_ADD:
        case IR_SUBTRACT:
        case IR_MULTIPLY:
        case IR_DIVIDE:
        case IR_MODULO: {
            const char *operation = "add";

            if (instruction->opcode == IR_SUBTRACT) {
                operation = "sub";
            } else if (instruction->opcode == IR_MULTIPLY) {
                operation = "mul";
            } else if (instruction->opcode == IR_DIVIDE) {
                operation = "sdiv";
            } else if (instruction->opcode == IR_MODULO) {
                operation = "srem";
            }

            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                left,
                sizeof(left)
            );

            emit_load_temporary(
                ctx,
                instruction->operand2,
                index + 100000,
                right,
                sizeof(right)
            );

            fprintf(
                ctx->output,
                "  %%arith%zu = %s i32 %s, %s\n",
                index,
                operation,
                left,
                right
            );

            snprintf(result, sizeof(result), "%%arith%zu", index);

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;
        }

        case IR_EQUAL:
        case IR_NOT_EQUAL:
        case IR_LESS:
        case IR_LESS_EQUAL:
        case IR_GREATER:
        case IR_GREATER_EQUAL: {
            const char *operation = "eq";

            switch (instruction->opcode) {
                case IR_EQUAL: operation = "eq"; break;
                case IR_NOT_EQUAL: operation = "ne"; break;
                case IR_LESS: operation = "slt"; break;
                case IR_LESS_EQUAL: operation = "sle"; break;
                case IR_GREATER: operation = "sgt"; break;
                case IR_GREATER_EQUAL: operation = "sge"; break;
                default: break;
            }

            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                left,
                sizeof(left)
            );

            emit_load_temporary(
                ctx,
                instruction->operand2,
                index + 100000,
                right,
                sizeof(right)
            );

            fprintf(
                ctx->output,
                "  %%cmp%zu = icmp %s i32 %s, %s\n",
                index,
                operation,
                left,
                right
            );

            fprintf(
                ctx->output,
                "  %%bool%zu = zext i1 %%cmp%zu to i32\n",
                index,
                index
            );

            snprintf(result, sizeof(result), "%%bool%zu", index);

            emit_store_temporary(
                ctx,
                instruction->result,
                result
            );
            break;
        }

        case IR_ARGUMENT:
            if (ctx->pending_count >= 8) {
                ctx->valid = false;
                break;
            }

            if (instruction->operand1 <= 0) {
                ctx->valid = false;
                break;
            }

            ctx->pending_arguments[ctx->pending_count++] =
                (size_t)instruction->operand1;
            break;

        case IR_CALL: {
            const SemanticSymbol *function =
                find_function(ctx->semantic, instruction->symbol_id);

            if (function == NULL ||
                ctx->pending_count != instruction->argument_count ||
                instruction->argument_count > 6) {
                ctx->valid = false;
                break;
            }

            /*
             * Load all argument values before emitting the call.
             */
            for (size_t i = 0; i < ctx->pending_count; i++) {
                fprintf(
                    ctx->output,
                    "  %%callarg%zu_%zu = load i32, ptr %%t%zu, align 4\n",
                    index,
                    i,
                    ctx->pending_arguments[i]
                );
            }

            fprintf(ctx->output, "  ");

            if (!function->is_void) {
                fprintf(ctx->output, "%%call%zu = ", index);
            }

            fprintf(
                ctx->output,
                "call %s @%s(",
                function->is_void ? "void" : "i32",
                instruction->name
            );

            for (size_t i = 0; i < ctx->pending_count; i++) {
                if (i > 0) {
                    fprintf(ctx->output, ", ");
                }

                fprintf(
                    ctx->output,
                    "i32 %%callarg%zu_%zu",
                    index,
                    i
                );
            }

            fprintf(ctx->output, ")\n");

            if (!function->is_void) {
                snprintf(result, sizeof(result), "%%call%zu", index);

                emit_store_temporary(
                    ctx,
                    instruction->result,
                    result
                );
            }

            ctx->pending_count = 0;
            break;
        }

        case IR_LABEL:
            if (!ctx->block_terminated) {
                fprintf(
                    ctx->output,
                    "  br label %%L%u\n",
                    instruction->label
                );
            }

            fprintf(
                ctx->output,
                "L%u:\n",
                instruction->label
            );
            ctx->block_terminated = false;
            break;

        case IR_JUMP:
            fprintf(
                ctx->output,
                "  br label %%L%u\n",
                instruction->label
            );
            ctx->block_terminated = true;
            break;

        case IR_BRANCH_ZERO:
        case IR_BRANCH_NONZERO:
            emit_load_temporary(
                ctx,
                instruction->operand1,
                index,
                left,
                sizeof(left)
            );

            fprintf(
                ctx->output,
                "  %%branch%zu = icmp %s i32 %s, 0\n",
                index,
                instruction->opcode == IR_BRANCH_ZERO ? "eq" : "ne",
                left
            );

            fprintf(
                ctx->output,
                "  br i1 %%branch%zu, label %%L%u, label %%fall%zu\n",
                index,
                instruction->opcode == IR_BRANCH_ZERO
                    ? instruction->label
                    : instruction->label,
                index
            );

            /*
             * Create an explicit fall-through block so that the
             * following Student IR instructions have a valid
             * LLVM basic block.
             */
            fprintf(ctx->output, "fall%zu:\n", index);
            ctx->block_terminated = false;
            break;

        case IR_RETURN:
            if (instruction->operand1 >= 0) {
                emit_load_temporary(
                    ctx,
                    instruction->operand1,
                    index,
                    result,
                    sizeof(result)
                );

                fprintf(
                    ctx->output,
                    "  ret i32 %s\n",
                    result
                );
            } else {
                fprintf(ctx->output, "  ret void\n");
            }

            ctx->block_terminated = true;
            break;

        default:
            ctx->valid = false;
            break;
    }
}

static int emit_function(
    LLVMContext *ctx,
    size_t begin,
    size_t end
)
{
    const IRInstruction *first =
        &ctx->program->instructions[begin];

    const SemanticSymbol *function =
        find_function(ctx->semantic, first->symbol_id);

    if (function == NULL || first->name == NULL) {
        return 0;
    }

    scan_function_symbols(ctx, begin, end);

    if (!ctx->valid) {
        return 0;
    }

    fprintf(
        ctx->output,
        "\ndefine %s @%s(",
        function->is_void ? "void" : "i32",
        first->name
    );

    for (size_t i = 0; i < function->parameter_count; i++) {
        if (i > 0) {
            fprintf(ctx->output, ", ");
        }

        fprintf(ctx->output, "i32 %%arg%zu", i);
    }

    fprintf(ctx->output, ") {\n");

    emit_allocations(ctx, begin, end);

    ctx->block_terminated = false;
    ctx->pending_count = 0;

    unsigned int parameter_index = 0;

    for (size_t i = begin + 1; i < end; i++) {
        emit_instruction(
            ctx,
            &ctx->program->instructions[i],
            i,
            &parameter_index
        );

        if (!ctx->valid) {
            break;
        }
    }

    if (ctx->pending_count != 0) {
        ctx->valid = false;
    }

    if (!ctx->block_terminated && ctx->valid) {
        fprintf(
            ctx->output,
            "  ret %s\n",
            function->is_void ? "void" : "i32 0"
        );
    }

    fprintf(ctx->output, "}\n");

    return ctx->valid ? 1 : 0;
}

int llvm_backend_emit(
    const IRProgram *program,
    const SemanticModel *semantic_model,
    FILE *output
)
{
    if (program == NULL ||
        semantic_model == NULL ||
        output == NULL ||
        !program->valid) {
        return 0;
    }

    LLVMContext ctx = {0};

    ctx.program = program;
    ctx.semantic = semantic_model;
    ctx.output = output;
    ctx.valid = true;

    for (size_t i = 0; i < semantic_model->symbol_count; i++) {
        const SemanticSymbol *symbol = semantic_model->symbols[i];

        if (symbol != NULL && symbol->id > ctx.max_symbol) {
            ctx.max_symbol = symbol->id;
        }
    }

    ctx.used_symbols = calloc(
        (size_t)ctx.max_symbol + 1,
        sizeof(bool)
    );

    if (ctx.used_symbols == NULL) {
        return 0;
    }

    fprintf(
        output,
        "; C Minor Compiler - generated LLVM IR\n"
        "source_filename = \"cminor\"\n"
    );

    size_t index = 0;

    while (index < program->instruction_count && ctx.valid) {
        const IRInstruction *instruction =
            &program->instructions[index];

        if (instruction->opcode != IR_FUNCTION_BEGIN) {
            index++;
            continue;
        }

        size_t end = index + 1;

        while (end < program->instruction_count &&
               program->instructions[end].opcode != IR_FUNCTION_END) {
            end++;
        }

        if (end >= program->instruction_count) {
            ctx.valid = false;
            break;
        }

        if (!emit_function(&ctx, index, end)) {
            ctx.valid = false;
            break;
        }

        index = end + 1;
    }

    free(ctx.used_symbols);

    return ctx.valid ? 1 : 0;
}
