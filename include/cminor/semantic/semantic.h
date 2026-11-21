#ifndef CMINOR_SEMANTIC_H
#define CMINOR_SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>

#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"

typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER
} SymbolKind;

typedef struct SemanticScope SemanticScope;

typedef struct SemanticSymbol {
    unsigned int id;

    SymbolKind kind;
    const char *name;

    bool is_void;
    bool declared;
    bool defined;

    size_t parameter_count;

    AstNode *definition;
    SemanticScope *scope;
} SemanticSymbol;

typedef struct {
    SemanticSymbol **symbols;
    size_t symbol_count;
    size_t symbol_capacity;

    SemanticScope **scopes;
    size_t scope_count;
    size_t scope_capacity;

    SemanticScope *global_scope;
    SemanticSymbol *main_function;

    bool valid;
} SemanticModel;

void semantic_model_init(SemanticModel *model);

int semantic_analyze(
    AstNode *program,
    DiagnosticEngine *diagnostics,
    SemanticModel *model
);

void semantic_dump_symbols(
    const SemanticModel *model,
    FILE *output
);

void semantic_model_destroy(SemanticModel *model);

#endif
