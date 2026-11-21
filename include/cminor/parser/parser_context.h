#ifndef CMINOR_PARSER_CONTEXT_H
#define CMINOR_PARSER_CONTEXT_H

#include "cminor/lexer/lexer.h"
#include "cminor/ast/ast.h"
#include "cminor/common/diagnostic.h"

typedef struct {
    Lexer *lexer;
    DiagnosticEngine *diagnostics;
    AstNode *program;
    int syntax_errors;
} ParserContext;

#endif
