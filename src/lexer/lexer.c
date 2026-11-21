#include <stdlib.h>

#include "cminor/lexer/lexer.h"
#include "cminor/lexer/lexer_internal.h"
#include "cminor/lexer/lexer_scanner.h"

struct Lexer {
    LexerContext context;
    yyscan_t scanner;
};

Lexer *lexer_create(
    FILE *input,
    const char *source_name,
    DiagnosticEngine *diagnostics
) {
    if (input == NULL || diagnostics == NULL) {
        return NULL;
    }

    Lexer *lexer = calloc(1, sizeof(Lexer));

    if (lexer == NULL) {
        return NULL;
    }

    lexer->context.input = input;
    lexer->context.source_name = source_name;
    lexer->context.diagnostics = diagnostics;

    lexer->context.line = 1;
    lexer->context.column = 1;
    lexer->context.offset = 0;

    lexer->context.token_start = (SourceLocation){1, 1, 0};
    lexer->context.token_end = (SourceLocation){1, 1, 0};

    if (cm_lex_init_extra(&lexer->context, &lexer->scanner) != 0) {
        free(lexer);
        return NULL;
    }

    cm_set_in(input, lexer->scanner);

    return lexer;
}

Token lexer_next_token(Lexer *lexer) {
    if (lexer == NULL) {
        return (Token){.kind = TOKEN_ERROR};
    }

    lexer->context.has_token = false;

    int result = cm_lex(lexer->scanner);

    if (result != 0 && lexer->context.has_token) {
        return lexer->context.current_token;
    }

    SourceLocation location = {
        lexer->context.line,
        lexer->context.column,
        lexer->context.offset
    };

    lexer->context.token_start = location;
    lexer->context.token_end = location;

    lexer_context_set_token(
        &lexer->context,
        TOKEN_EOF,
        "",
        0,
        0
    );

    return lexer->context.current_token;
}

void lexer_destroy(Lexer *lexer) {
    if (lexer == NULL) {
        return;
    }

    cm_lex_destroy(lexer->scanner);
    free(lexer);
}
