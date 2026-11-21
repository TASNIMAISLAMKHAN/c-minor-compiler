#ifndef CMINOR_LEXER_H
#define CMINOR_LEXER_H

#include <stdio.h>
#include "cminor/lexer/token.h"
#include "cminor/common/diagnostic.h"

typedef struct Lexer Lexer;

Lexer *lexer_create(FILE *input,
                    const char *source_name,
                    DiagnosticEngine *diagnostics);

Token lexer_next_token(Lexer *lexer);

void lexer_destroy(Lexer *lexer);

#endif
