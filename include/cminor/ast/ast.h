#ifndef CMINOR_AST_H
#define CMINOR_AST_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "cminor/common/source_location.h"
#include "cminor/lexer/token.h"

struct SemanticSymbol;

typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_PARAMETER,
    AST_BLOCK,
    AST_VARIABLE_DECLARATION,
    AST_EXPRESSION_STATEMENT,
    AST_EMPTY_STATEMENT,
    AST_IF_STATEMENT,
    AST_WHILE_STATEMENT,
    AST_FOR_STATEMENT,
    AST_BREAK_STATEMENT,
    AST_CONTINUE_STATEMENT,
    AST_RETURN_STATEMENT,

    AST_INTEGER,
    AST_IDENTIFIER,
    AST_CALL,
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_UNARY,
    AST_GROUP
} AstKind;

typedef struct AstNode AstNode;

struct AstNode {
    AstKind kind;
    SourceSpan span;

    char *name;
    int32_t integer_value;
    int operation;

    bool is_definition;
    bool is_void;

    struct SemanticSymbol *symbol;

    AstNode *first;
    AstNode *second;
    AstNode *third;

    AstNode *children;
    AstNode *next;
};

AstNode *ast_new(AstKind kind, SourceSpan span);
AstNode *ast_new_named(AstKind kind, SourceSpan span, char *name);
AstNode *ast_new_integer(SourceSpan span, int32_t value);

AstNode *ast_new_operation(
    AstKind kind,
    SourceSpan span,
    int operation,
    AstNode *left,
    AstNode *right
);

AstNode *ast_list_append(AstNode *list, AstNode *node);

void ast_dump(const AstNode *node, FILE *output);
void ast_free(AstNode *node);

#endif
