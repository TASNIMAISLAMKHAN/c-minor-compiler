#ifndef CMINOR_LEXER_INTERNAL_H
#define CMINOR_LEXER_INTERNAL_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cminor/lexer/token.h"
#include "cminor/common/diagnostic.h"

typedef struct {
    FILE *input;
    const char *source_name;
    DiagnosticEngine *diagnostics;

    size_t line;
    size_t column;
    size_t offset;

    SourceLocation token_start;
    SourceLocation token_end;
    SourceLocation comment_start;

    Token current_token;
    bool has_token;
} LexerContext;

void lexer_context_advance(
    LexerContext *context,
    const char *text,
    size_t length
);

void lexer_context_set_token(
    LexerContext *context,
    TokenKind kind,
    const char *text,
    size_t length,
    int32_t integer_value
);

#endif
