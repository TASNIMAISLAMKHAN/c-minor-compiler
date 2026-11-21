#include <stdlib.h>
#include <string.h>

#include "cminor/lexer/lexer_internal.h"

void lexer_context_advance(
    LexerContext *context,
    const char *text,
    size_t length
) {
    if (context == NULL || text == NULL) {
        return;
    }

    context->token_start.line = context->line;
    context->token_start.column = context->column;
    context->token_start.offset = context->offset;

    size_t i = 0;

    while (i < length) {
        unsigned char ch = (unsigned char)text[i];

        if (ch == '\r') {
            if (i + 1 < length && text[i + 1] == '\n') {
                i++;
                context->offset++;
            }

            context->line++;
            context->column = 1;
        } else if (ch == '\n') {
            context->line++;
            context->column = 1;
        } else {
            context->column++;
        }

        context->offset++;
        i++;
    }

    context->token_end.line = context->line;
    context->token_end.column = context->column;
    context->token_end.offset = context->offset;
}

void lexer_context_set_token(
    LexerContext *context,
    TokenKind kind,
    const char *text,
    size_t length,
    int32_t integer_value
) {
    if (context == NULL) {
        return;
    }

    char *lexeme = malloc(length + 1);

    if (lexeme != NULL) {
        if (text != NULL && length > 0) {
            memcpy(lexeme, text, length);
        }

        lexeme[length] = '\0';
    }

    context->current_token.kind = kind;
    context->current_token.lexeme = lexeme;
    context->current_token.integer_value = integer_value;

    context->current_token.span.start = context->token_start;
    context->current_token.span.end = context->token_end;

    context->has_token = true;
}
