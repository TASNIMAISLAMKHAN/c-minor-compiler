#include "cminor/semantic/semantic.h"

#include <stdlib.h>
#include <string.h>

struct SemanticScope {
    unsigned int id;
    struct SemanticScope *parent;

    SemanticSymbol **symbols;
    size_t symbol_count;
    size_t symbol_capacity;
};

typedef enum {
    TYPE_ERROR,
    TYPE_INT,
    TYPE_VOID
} ExpressionType;

typedef struct {
    SemanticModel *model;
    DiagnosticEngine *diagnostics;
    SemanticScope *current_scope;
    SemanticSymbol *current_function;
    unsigned int next_symbol_id;
    unsigned int next_scope_id;
    unsigned int loop_depth;
} SemanticContext;

/* ---------------------------------------------------------
   Dynamic arrays and model management
   --------------------------------------------------------- */

static int append_pointer(void ***array, size_t *count,
                          size_t *capacity, void *item)
{
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);

        void **new_array = realloc(*array, new_capacity * sizeof(void *));
        if (new_array == NULL) {
            return 0;
        }

        *array = new_array;
        *capacity = new_capacity;
    }

    (*array)[(*count)++] = item;
    return 1;
}

void semantic_model_init(SemanticModel *model)
{
    if (model != NULL) {
        memset(model, 0, sizeof(*model));
    }
}

static void semantic_error(
    SemanticContext *ctx,
    SourceSpan span,
    const char *message
)
{
    diagnostic_report(
        ctx->diagnostics,
        DIAGNOSTIC_ERROR,
        span,
        "%s",
        message
    );

    ctx->model->valid = false;
}

static SemanticScope *create_scope(
    SemanticContext *ctx,
    SemanticScope *parent
)
{
    SemanticScope *scope = calloc(1, sizeof(*scope));

    if (scope == NULL) {
        return NULL;
    }

    scope->id = ++ctx->next_scope_id;
    scope->parent = parent;

    if (!append_pointer(
            (void ***)&ctx->model->scopes,
            &ctx->model->scope_count,
            &ctx->model->scope_capacity,
            scope)) {
        free(scope);
        return NULL;
    }

    return scope;
}

static SemanticSymbol *create_symbol(
    SemanticContext *ctx,
    SemanticScope *scope,
    SymbolKind kind,
    const char *name
)
{
    SemanticSymbol *symbol = calloc(1, sizeof(*symbol));

    if (symbol == NULL) {
        return NULL;
    }

    symbol->id = ++ctx->next_symbol_id;
    symbol->kind = kind;
    symbol->name = name;
    symbol->scope = scope;

    if (!append_pointer(
            (void ***)&ctx->model->symbols,
            &ctx->model->symbol_count,
            &ctx->model->symbol_capacity,
            symbol)) {
        free(symbol);
        return NULL;
    }

    if (!append_pointer(
            (void ***)&scope->symbols,
            &scope->symbol_count,
            &scope->symbol_capacity,
            symbol)) {
        ctx->model->symbol_count--;
        free(symbol);
        return NULL;
    }

    return symbol;
}

/* ---------------------------------------------------------
   Symbol lookup
   --------------------------------------------------------- */

static SemanticSymbol *lookup_in_scope(
    SemanticScope *scope,
    const char *name,
    SymbolKind kind
)
{
    if (scope == NULL || name == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < scope->symbol_count; i++) {
        SemanticSymbol *symbol = scope->symbols[i];

        if (symbol->kind == kind &&
            strcmp(symbol->name, name) == 0) {
            return symbol;
        }
    }

    return NULL;
}

static SemanticSymbol *lookup_variable(
    SemanticScope *scope,
    const char *name
)
{
    while (scope != NULL) {
        SemanticSymbol *symbol =
            lookup_in_scope(scope, name, SYMBOL_VARIABLE);

        if (symbol == NULL) {
            symbol = lookup_in_scope(scope, name, SYMBOL_PARAMETER);
        }

        if (symbol != NULL) {
            return symbol;
        }

        scope = scope->parent;
    }

    return NULL;
}

static SemanticSymbol *lookup_function(
    SemanticModel *model,
    const char *name
)
{
    if (model->global_scope == NULL) {
        return NULL;
    }

    return lookup_in_scope(
        model->global_scope,
        name,
        SYMBOL_FUNCTION
    );
}

/* ---------------------------------------------------------
   Declaration handling
   --------------------------------------------------------- */

static SemanticSymbol *declare_variable(
    SemanticContext *ctx,
    AstNode *node,
    SymbolKind kind
)
{
    if (node == NULL || node->name == NULL) {
        return NULL;
    }

    if (lookup_in_scope(ctx->current_scope, node->name, SYMBOL_VARIABLE) ||
        lookup_in_scope(ctx->current_scope, node->name, SYMBOL_PARAMETER)) {
        semantic_error(ctx, node->span, "Duplicate variable declaration.");
        return NULL;
    }

    if (lookup_function(ctx->model, node->name) != NULL) {
        semantic_error(
            ctx,
            node->span,
            "A function name cannot be used as a variable name."
        );
        return NULL;
    }

    SemanticSymbol *symbol = create_symbol(
        ctx,
        ctx->current_scope,
        kind,
        node->name
    );

    if (symbol == NULL) {
        semantic_error(ctx, node->span, "Memory allocation failed.");
        return NULL;
    }

    node->symbol = symbol;
    symbol->declared = true;

    return symbol;
}

/* ---------------------------------------------------------
   Expression analysis
   --------------------------------------------------------- */

static ExpressionType check_expression(
    SemanticContext *ctx,
    AstNode *node
);

static ExpressionType require_integer(
    SemanticContext *ctx,
    AstNode *node
)
{
    ExpressionType type = check_expression(ctx, node);

    if (type == TYPE_VOID) {
        semantic_error(
            ctx,
            node->span,
            "A void expression cannot be used where an integer is required."
        );
        return TYPE_ERROR;
    }

    return type;
}

static ExpressionType check_call(
    SemanticContext *ctx,
    AstNode *node
)
{
    SemanticSymbol *function =
        lookup_function(ctx->model, node->name);

    size_t argument_count = 0;

    for (AstNode *argument = node->children;
         argument != NULL;
         argument = argument->next) {
        require_integer(ctx, argument);
        argument_count++;
    }

    if (function == NULL) {
        semantic_error(
            ctx,
            node->span,
            "Call to an undeclared function."
        );
        return TYPE_ERROR;
    }

    node->symbol = function;

    if (argument_count != function->parameter_count) {
        semantic_error(
            ctx,
            node->span,
            "Incorrect number of function arguments."
        );
    }

    return function->is_void ? TYPE_VOID : TYPE_INT;
}

static ExpressionType check_expression(
    SemanticContext *ctx,
    AstNode *node
)
{
    if (node == NULL) {
        return TYPE_ERROR;
    }

    switch (node->kind) {
        case AST_INTEGER:
            return TYPE_INT;

        case AST_IDENTIFIER: {
            SemanticSymbol *symbol =
                lookup_variable(ctx->current_scope, node->name);

            if (symbol == NULL) {
                semantic_error(
                    ctx,
                    node->span,
                    "Use of an undeclared variable."
                );
                return TYPE_ERROR;
            }

            node->symbol = symbol;
            return TYPE_INT;
        }

        case AST_CALL:
            return check_call(ctx, node);

        case AST_ASSIGNMENT: {
            if (node->first == NULL ||
                node->first->kind != AST_IDENTIFIER) {
                semantic_error(
                    ctx,
                    node->span,
                    "The left side of an assignment must be a variable."
                );

                require_integer(ctx, node->second);
                return TYPE_ERROR;
            }

            ExpressionType left =
                check_expression(ctx, node->first);

            ExpressionType right =
                require_integer(ctx, node->second);

            if (left == TYPE_VOID || right == TYPE_VOID) {
                return TYPE_ERROR;
            }

            return TYPE_INT;
        }

        case AST_BINARY:
            require_integer(ctx, node->first);
            require_integer(ctx, node->second);
            return TYPE_INT;

        case AST_UNARY:
            require_integer(ctx, node->first);
            return TYPE_INT;

        case AST_GROUP:
            return check_expression(ctx, node->first);

        default:
            semantic_error(
                ctx,
                node->span,
                "Invalid expression node."
            );
            return TYPE_ERROR;
    }
}

/* ---------------------------------------------------------
   Statement analysis
   --------------------------------------------------------- */

static void analyze_statement(
    SemanticContext *ctx,
    AstNode *node
);

static void analyze_block(
    SemanticContext *ctx,
    AstNode *block,
    bool create_new_scope
)
{
    if (block == NULL) {
        return;
    }

    SemanticScope *previous_scope = ctx->current_scope;

    if (create_new_scope) {
        ctx->current_scope =
            create_scope(ctx, previous_scope);

        if (ctx->current_scope == NULL) {
            semantic_error(
                ctx,
                block->span,
                "Unable to create a local scope."
            );
            ctx->current_scope = previous_scope;
            return;
        }
    }

    for (AstNode *item = block->children;
         item != NULL;
         item = item->next) {
        analyze_statement(ctx, item);
    }

    ctx->current_scope = previous_scope;
}

static void analyze_variable_declaration(
    SemanticContext *ctx,
    AstNode *node
)
{
    SemanticSymbol *symbol =
        declare_variable(ctx, node, SYMBOL_VARIABLE);

    if (node->first != NULL) {
        require_integer(ctx, node->first);
    }

    if (symbol != NULL) {
        symbol->defined = true;
    }
}

static void analyze_statement(
    SemanticContext *ctx,
    AstNode *node
)
{
    if (node == NULL) {
        return;
    }

    switch (node->kind) {
        case AST_BLOCK:
            analyze_block(ctx, node, true);
            break;

        case AST_VARIABLE_DECLARATION:
            analyze_variable_declaration(ctx, node);
            break;

        case AST_EXPRESSION_STATEMENT:
            if (node->first != NULL) {
                check_expression(ctx, node->first);
            }
            break;

        case AST_EMPTY_STATEMENT:
            break;

        case AST_IF_STATEMENT:
            require_integer(ctx, node->first);
            analyze_statement(ctx, node->second);

            if (node->third != NULL) {
                analyze_statement(ctx, node->third);
            }
            break;

        case AST_WHILE_STATEMENT:
            require_integer(ctx, node->first);

            ctx->loop_depth++;
            analyze_statement(ctx, node->second);
            ctx->loop_depth--;
            break;

        case AST_FOR_STATEMENT: {
            SemanticScope *previous_scope = ctx->current_scope;

            ctx->current_scope =
                create_scope(ctx, previous_scope);

            if (ctx->current_scope == NULL) {
                semantic_error(
                    ctx,
                    node->span,
                    "Unable to create a for-loop scope."
                );
                ctx->current_scope = previous_scope;
                break;
            }

            /* first: initializer, second: condition,
               third: increment, children: loop body */

            if (node->first != NULL) {
                analyze_statement(ctx, node->first);
            }

            if (node->second != NULL) {
                require_integer(ctx, node->second);
            }

            if (node->third != NULL) {
                check_expression(ctx, node->third);
            }

            ctx->loop_depth++;
            analyze_statement(ctx, node->children);
            ctx->loop_depth--;

            ctx->current_scope = previous_scope;
            break;
        }

        case AST_BREAK_STATEMENT:
            if (ctx->loop_depth == 0) {
                semantic_error(
                    ctx,
                    node->span,
                    "'break' can only be used inside a loop."
                );
            }
            break;

        case AST_CONTINUE_STATEMENT:
            if (ctx->loop_depth == 0) {
                semantic_error(
                    ctx,
                    node->span,
                    "'continue' can only be used inside a loop."
                );
            }
            break;

        case AST_RETURN_STATEMENT:
            if (ctx->current_function == NULL) {
                semantic_error(
                    ctx,
                    node->span,
                    "Return statement outside a function."
                );
                break;
            }

            if (ctx->current_function->is_void) {
                if (node->first != NULL) {
                    semantic_error(
                        ctx,
                        node->span,
                        "A void function cannot return a value."
                    );
                    require_integer(ctx, node->first);
                }
            } else {
                if (node->first == NULL) {
                    semantic_error(
                        ctx,
                        node->span,
                        "An int function must return an integer value."
                    );
                } else {
                    require_integer(ctx, node->first);
                }
            }
            break;

        default:
            semantic_error(
                ctx,
                node->span,
                "Invalid statement node."
            );
            break;
    }
}

/* ---------------------------------------------------------
   Function analysis
   --------------------------------------------------------- */

static size_t count_parameters(AstNode *parameters)
{
    size_t count = 0;

    for (AstNode *parameter = parameters;
         parameter != NULL;
         parameter = parameter->next) {
        count++;
    }

    return count;
}

static void analyze_function(
    SemanticContext *ctx,
    AstNode *function_node
)
{
    if (function_node->name == NULL) {
        semantic_error(
            ctx,
            function_node->span,
            "Function declaration has no name."
        );
        return;
    }

    size_t parameter_count =
        count_parameters(function_node->first);

    if (parameter_count > 6) {
        semantic_error(
            ctx,
            function_node->span,
            "A function cannot have more than six parameters."
        );
    }

    SemanticSymbol *symbol =
        lookup_function(ctx->model, function_node->name);

    if (symbol == NULL) {
        if (lookup_variable(ctx->model->global_scope,
                            function_node->name) != NULL) {
            semantic_error(
                ctx,
                function_node->span,
                "Function name conflicts with a variable."
            );
            return;
        }

        symbol = create_symbol(
            ctx,
            ctx->model->global_scope,
            SYMBOL_FUNCTION,
            function_node->name
        );

        if (symbol == NULL) {
            semantic_error(
                ctx,
                function_node->span,
                "Memory allocation failed."
            );
            return;
        }

        symbol->declared = true;
        symbol->is_void = function_node->is_void;
        symbol->parameter_count = parameter_count;
    } else {
        if (symbol->is_void != function_node->is_void ||
            symbol->parameter_count != parameter_count) {
            semantic_error(
                ctx,
                function_node->span,
                "Function declaration does not match its previous declaration."
            );
        }
    }

    function_node->symbol = symbol;

    if (!function_node->is_definition) {
        return;
    }

    if (symbol->defined) {
        semantic_error(
            ctx,
            function_node->span,
            "Function has more than one definition."
        );
        return;
    }

    symbol->defined = true;
    symbol->definition = function_node;

    if (strcmp(function_node->name, "main") == 0) {
        ctx->model->main_function = symbol;

        if (function_node->is_void || parameter_count != 0) {
            semantic_error(
                ctx,
                function_node->span,
                "The main function must have the signature int main(void)."
            );
        }
    }

    SemanticScope *function_scope =
        create_scope(ctx, ctx->model->global_scope);

    if (function_scope == NULL) {
        semantic_error(
            ctx,
            function_node->span,
            "Unable to create function scope."
        );
        return;
    }

    SemanticScope *previous_scope = ctx->current_scope;
    SemanticSymbol *previous_function = ctx->current_function;

    ctx->current_scope = function_scope;
    ctx->current_function = symbol;

    for (AstNode *parameter = function_node->first;
         parameter != NULL;
         parameter = parameter->next) {

        if (parameter->name == NULL) {
            semantic_error(
                ctx,
                parameter->span,
                "Function definition parameters must have names."
            );
            continue;
        }

        declare_variable(ctx, parameter, SYMBOL_PARAMETER);
    }

    AstNode *body = function_node->second;

    if (body != NULL && body->kind == AST_BLOCK) {
        /* Function body shares its scope with parameters. */
        analyze_block(ctx, body, false);

        if (!function_node->is_void) {
            AstNode *last = body->children;

            while (last != NULL && last->next != NULL) {
                last = last->next;
            }

            if (last == NULL ||
                last->kind != AST_RETURN_STATEMENT ||
                last->first == NULL) {
                semantic_error(
                    ctx,
                    body->span,
                    "An int function must end with an explicit top-level return expression."
                );
            }
        }
    }

    ctx->current_scope = previous_scope;
    ctx->current_function = previous_function;
}

/* ---------------------------------------------------------
   Public semantic analysis interface
   --------------------------------------------------------- */

int semantic_analyze(
    AstNode *program,
    DiagnosticEngine *diagnostics,
    SemanticModel *model
)
{
    if (program == NULL || diagnostics == NULL || model == NULL) {
        return 0;
    }

    semantic_model_init(model);
    model->valid = true;

    SemanticContext ctx = {0};
    ctx.model = model;
    ctx.diagnostics = diagnostics;

    unsigned int initial_errors = diagnostics->stats.error_count;

    model->global_scope = create_scope(&ctx, NULL);

    if (model->global_scope == NULL) {
        model->valid = false;
        return 0;
    }

    ctx.current_scope = model->global_scope;

    if (program->kind != AST_PROGRAM) {
        semantic_error(
            &ctx,
            program->span,
            "The AST root is not a program node."
        );
        return 0;
    }

    /* Process functions in source order.
       A function must be declared before it is called. */

    for (AstNode *function = program->children;
         function != NULL;
         function = function->next) {
        if (function->kind == AST_FUNCTION) {
            analyze_function(&ctx, function);
        } else {
            semantic_error(
                &ctx,
                function->span,
                "Only function declarations and definitions are allowed at file scope."
            );
        }
    }

    if (model->main_function == NULL) {
        semantic_error(
            &ctx,
            program->span,
            "Program must contain exactly one int main(void) definition."
        );
    }

    for (size_t i = 0; i < model->symbol_count; i++) {
        SemanticSymbol *symbol = model->symbols[i];

        if (symbol->kind == SYMBOL_FUNCTION && !symbol->defined) {
            semantic_error(
                &ctx,
                program->span,
                "A declared function has no definition in this source file."
            );
        }
    }

    if (diagnostics->stats.error_count > initial_errors) {
        model->valid = false;
    }

    return model->valid ? 1 : 0;
}

/* ---------------------------------------------------------
   Symbol table output
   --------------------------------------------------------- */

void semantic_dump_symbols(
    const SemanticModel *model,
    FILE *output
)
{
    if (model == NULL || output == NULL) {
        return;
    }

    fprintf(output, "\n========== SYMBOL TABLE ==========\n");

    for (size_t i = 0; i < model->symbol_count; i++) {
        const SemanticSymbol *symbol = model->symbols[i];

        const char *kind_name = "UNKNOWN";

        switch (symbol->kind) {
            case SYMBOL_FUNCTION:
                kind_name = "FUNCTION";
                break;

            case SYMBOL_VARIABLE:
                kind_name = "VARIABLE";
                break;

            case SYMBOL_PARAMETER:
                kind_name = "PARAMETER";
                break;
        }

        fprintf(
            output,
            "ID: %-4u Name: %-16s Kind: %-10s Scope: %-4u",
            symbol->id,
            symbol->name,
            kind_name,
            symbol->scope != NULL ? symbol->scope->id : 0
        );

        if (symbol->kind == SYMBOL_FUNCTION) {
            fprintf(
                output,
                " Return: %-5s Parameters: %zu Defined: %s",
                symbol->is_void ? "void" : "int",
                symbol->parameter_count,
                symbol->defined ? "yes" : "no"
            );
        }

        fprintf(output, "\n");
    }

    fprintf(output, "==================================\n");
}

/* ---------------------------------------------------------
   Cleanup
   --------------------------------------------------------- */

void semantic_model_destroy(SemanticModel *model)
{
    if (model == NULL) {
        return;
    }

    for (size_t i = 0; i < model->scope_count; i++) {
        SemanticScope *scope = model->scopes[i];

        free(scope->symbols);
        free(scope);
    }

    for (size_t i = 0; i < model->symbol_count; i++) {
        free(model->symbols[i]);
    }

    free(model->scopes);
    free(model->symbols);

    memset(model, 0, sizeof(*model));
}
